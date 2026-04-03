#include "goal_relevance_pass.hpp"
#include "../abstraction/achievers_analysis.hpp"
#include "../analysis/numeric_relaxed_planning_graph.hpp"
#include "../config/config.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"

#include <algorithm>
#include <chrono>
#include <set>

namespace rantanplan {

void GoalRelevancePass::apply(PipelineResult& result) const {
    if (!Config::instance().rpg.goal_relevance) {
        return;
    }

    auto& stats = Stats::instance();
    auto pass_start = std::chrono::high_resolution_clock::now();

    // Determine whether cost expression fluents matter for relevance.
    // In satisficing mode, any valid plan suffices — cost doesn't matter.
    // In optimal/anytime, removing an action that only affects costs
    // could eliminate the optimal plan.
    SearchMode mode = parse_search_mode(Config::instance().planner.mode);
    bool include_cost_fluents = (mode == SearchMode::Optimal || mode == SearchMode::Anytime);

    size_t initial_actions = result.problem.actions().size();
    int iteration = 0;
    std::unique_ptr<AchieversAnalysis> final_achievers;

    while (iteration < MAX_ITERATIONS) {
        iteration++;

        // Step 1: Build NumericRPG on current problem
        NumericRelaxedPlanningGraph rpg(result.problem);
        rpg.set_stop_when_all_reachable(false);  // Fixpoint for widest bounds
        rpg.build();

        AchieversAnalysis::RPGData rpg_data;
        rpg_data.state_variable_bounds = rpg.get_state_variable_bounds();
        rpg_data.action_first_layers = rpg.get_action_first_layers();
        rpg_data.layer_count = static_cast<int>(rpg.get_layer_count());

        // Update lower bound (RPG on the reduced problem may give a higher bound)
        if (rpg.are_goals_achievable()) {
            result.lower_bound = std::max(result.lower_bound,
                                          rpg.get_minimum_steps_lower_bound());
        }

        // Step 2: Run AchieversAnalysis with pre-computed RPG data
        auto achievers = std::make_unique<AchieversAnalysis>(
            result.problem, std::move(rpg_data));

        // Step 3: Compute goal-relevant action indices
        auto relevant_indices = achievers->compute_goal_relevant_action_indices(
            result.problem, include_cost_fluents);

        // Compute which actions to remove (complement of relevant)
        std::vector<size_t> removed_indices;
        for (size_t i = 0; i < result.problem.actions().size(); ++i) {
            if (!relevant_indices.count(i)) {
                removed_indices.push_back(i);
            }
        }

        Logger::instance().component(VerbosityLevel::INFO, "GoalRelevance", {
            {"iteration", std::to_string(iteration)},
            {"relevant", std::to_string(relevant_indices.size()) + "/" +
                         std::to_string(result.problem.actions().size())},
            {"removing", std::to_string(removed_indices.size())}
        });

        // Log removed action names (verbose only)
        if (Config::instance().is_verbose()) {
            for (size_t idx : removed_indices) {
                Logger::instance().info("  GoalRelevance removed: " +
                    result.problem.actions()[idx].label());
            }
        }

        if (removed_indices.empty()) {
            // Fixpoint reached — no more actions to remove
            final_achievers = std::move(achievers);
            break;
        }

        // Step 4: Remove non-relevant actions
        result.problem = result.problem.without_actions(removed_indices);

        // Store achievers for potential reuse (will be overwritten if loop continues)
        final_achievers = std::move(achievers);
    }

    // Store the final achiever analysis for the planner to reuse.
    // Note: after action removal, the achiever data references actions by ID,
    // which remains stable across without_actions() calls. However, if actions
    // were removed in the last iteration, the achievers were computed on the
    // pre-removal problem. We need to re-run on the final problem.
    //
    // The loop guarantees that when we exit:
    //   - If removed_indices was empty: achievers matches the final problem (correct)
    //   - If we hit MAX_ITERATIONS: achievers was built before the last removal,
    //     so we need one final achiever computation on the final problem.
    if (iteration == MAX_ITERATIONS) {
        // Rebuild achievers on the final (reduced) problem
        NumericRelaxedPlanningGraph rpg(result.problem);
        rpg.set_stop_when_all_reachable(false);
        rpg.build();

        AchieversAnalysis::RPGData rpg_data;
        rpg_data.state_variable_bounds = rpg.get_state_variable_bounds();
        rpg_data.action_first_layers = rpg.get_action_first_layers();
        rpg_data.layer_count = static_cast<int>(rpg.get_layer_count());

        result.lower_bound = std::max(result.lower_bound,
                                      rpg.get_minimum_steps_lower_bound());

        final_achievers = std::make_unique<AchieversAnalysis>(
            result.problem, std::move(rpg_data));
    }

    result.achievers = std::move(final_achievers);

    auto pass_end = std::chrono::high_resolution_clock::now();
    double pass_time = std::chrono::duration<double>(pass_end - pass_start).count();

    size_t final_actions = result.problem.actions().size();
    size_t total_removed = initial_actions - final_actions;

    Logger::instance().component(VerbosityLevel::INFO, "GoalRelevance", {
        {"total removed", std::to_string(total_removed) + "/" + std::to_string(initial_actions)},
        {"iterations", std::to_string(iteration)},
        {"time", std::to_string(pass_time) + "s"}
    });

    stats.set("goal_relevance.initial_actions", static_cast<double>(initial_actions));
    stats.set("goal_relevance.final_actions", static_cast<double>(final_actions));
    stats.set("goal_relevance.removed_actions", static_cast<double>(total_removed));
    stats.set("goal_relevance.iterations", static_cast<double>(iteration));
    stats.set("goal_relevance.time_seconds", pass_time);
}

} // namespace rantanplan
