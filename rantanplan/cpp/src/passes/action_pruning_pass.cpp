#include "action_pruning_pass.hpp"
#include "bound_tightening.hpp"
#include "../abstraction/achievers_analysis.hpp"
#include "../analysis/numeric_relaxed_planning_graph.hpp"
#include "../config/config.hpp"
#include "../util/logger.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/stats.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <set>
#include <sstream>

namespace rantanplan {

void ActionPruningPass::apply(PipelineResult& result) const {
    auto& config = Config::instance();

    bool need_rpg = config.rpg.enabled;
    bool need_goal_relevance = config.rpg.goal_relevance;
    bool need_bounds = config.local_reasoning.enabled &&
                       config.local_reasoning.rpg_bounds;

    if (!need_rpg && !need_goal_relevance && !need_bounds) {
        return;
    }

    ScopedTimer pass_timer("pass.action-pruning.time_ms");
    auto& stats = Stats::instance();

    SearchMode mode = parse_search_mode(config.planner.mode);
    bool include_cost_fluents = (mode == SearchMode::Optimal ||
                                 mode == SearchMode::Anytime);

    int max_iter = need_goal_relevance
        ? config.rpg.goal_relevance_max_iterations : 1;

    size_t initial_actions = result.problem.actions().size();
    std::unique_ptr<AchieversAnalysis> final_achievers;
    std::unique_ptr<AchieversAnalysis> achievers;  // persists across iterations for incremental reuse
    std::unordered_map<ExprID, Interval> final_bounds;
    std::vector<ConservationConstraint> final_conservations;
    int iteration = 0;

    while (iteration < max_iter) {
        iteration++;

        // 1. Build RPG (always fixpoint for tightest bounds)
        auto rpg = std::make_unique<NumericRelaxedPlanningGraph>(result.problem);
        rpg->set_stop_when_all_reachable(false);
        bool goals_reachable = rpg->build();

        if (!goals_reachable) {
            result.proven_unsolvable = true;
            result.unsolvable_reason = name();
            return;
        }

        result.lower_bound = std::max(result.lower_bound,
                                       rpg->get_minimum_steps_lower_bound());

        // 2. Extract RPG data
        AchieversAnalysis::RPGData rpg_data;
        rpg_data.state_variable_bounds = rpg->get_state_variable_bounds();
        rpg_data.action_first_layers = rpg->get_action_first_layers();
        rpg_data.layer_count = static_cast<int>(rpg->get_layer_count());

        // 3. Bound tightening + conservation law discovery
        std::vector<ConservationConstraint> conservations;
        if (need_bounds) {
            int n = tighten_bounds_syntactically(
                rpg_data.state_variable_bounds, result.problem);
            conservations = discover_conservation_laws(result.problem);
            derive_bounds_from_conservations(
                rpg_data.state_variable_bounds, conservations);
            if (n > 0 || !conservations.empty()) {
                Logger::instance().info("  bounds tightened: " + std::to_string(n) +
                    ", conservation laws: " + std::to_string(conservations.size()));
            }
        }

        // 4. Forward prune: remove RPG-unreachable actions
        if (config.rpg.action_removal) {
            auto rpg_removable = rpg->get_removable_action_indices();
            if (!rpg_removable.empty()) {
                result.problem = result.problem.without_actions(rpg_removable);
            }
        }

        // Store RPG for downstream reuse
        result.fixpoint_rpg = std::move(rpg);

        // 5. Backward prune: goal-relevance analysis
        if (need_goal_relevance) {
            // Capture bounds BEFORE moving rpg_data into AchieversAnalysis
            auto bounds_snapshot = rpg_data.state_variable_bounds;

            // First iteration: construct. Subsequent: incremental update.
            if (!achievers) {
                achievers = std::make_unique<AchieversAnalysis>(
                    result.problem, std::move(rpg_data));
            } else {
                achievers->update(result.problem, std::move(rpg_data));
            }

            auto relevant_indices = achievers->compute_goal_relevant_action_indices(
                result.problem, include_cost_fluents);

            std::vector<size_t> removed_indices;
            for (size_t i = 0; i < result.problem.actions().size(); ++i) {
                if (!relevant_indices.count(i)) {
                    removed_indices.push_back(i);
                }
            }

            Logger::instance().component(VerbosityLevel::INFO, "ActionPruning", {
                {"iteration", std::to_string(iteration)},
                {"relevant", std::to_string(relevant_indices.size()) + "/" +
                             std::to_string(result.problem.actions().size())},
                {"removing", std::to_string(removed_indices.size())}
            });

            if (Config::instance().is_verbose()) {
                for (size_t idx : removed_indices) {
                    Logger::instance().info("  ActionPruning removed: " +
                        result.problem.actions()[idx].label());
                }
            }

            if (removed_indices.empty()) {
                // Fixpoint reached — capture final data
                final_achievers = std::move(achievers);
                final_bounds = std::move(bounds_snapshot);
                final_conservations = std::move(conservations);
                break;
            }

            result.problem = result.problem.without_actions(removed_indices);
            // achievers survives for incremental update on next iteration
        } else {
            // No backward pruning — single iteration, capture final data
            final_bounds = std::move(rpg_data.state_variable_bounds);
            final_conservations = std::move(conservations);
            break;
        }
    }

    // If we hit max_iterations with goal_relevance, do one final update
    if (iteration == max_iter && need_goal_relevance) {
        auto rpg = std::make_unique<NumericRelaxedPlanningGraph>(result.problem);
        rpg->set_stop_when_all_reachable(false);
        rpg->build();

        AchieversAnalysis::RPGData rpg_data;
        rpg_data.state_variable_bounds = rpg->get_state_variable_bounds();
        rpg_data.action_first_layers = rpg->get_action_first_layers();
        rpg_data.layer_count = static_cast<int>(rpg->get_layer_count());

        std::vector<ConservationConstraint> conservations;
        if (need_bounds) {
            tighten_bounds_syntactically(
                rpg_data.state_variable_bounds, result.problem);
            conservations = discover_conservation_laws(result.problem);
            derive_bounds_from_conservations(
                rpg_data.state_variable_bounds, conservations);
        }

        result.lower_bound = std::max(result.lower_bound,
                                       rpg->get_minimum_steps_lower_bound());
        result.fixpoint_rpg = std::move(rpg);

        // Capture bounds before moving rpg_data
        final_bounds = rpg_data.state_variable_bounds;
        final_conservations = std::move(conservations);

        if (achievers) {
            achievers->update(result.problem, std::move(rpg_data));
            final_achievers = std::move(achievers);
        } else {
            final_achievers = std::make_unique<AchieversAnalysis>(
                result.problem, std::move(rpg_data));
        }
    }

    // Compute SDAC cost bounds on the FINAL problem (fixes alignment bug:
    // bounds are now indexed to the final action set, not a pre-pruning one)
    bool need_sdac_bounds = result.problem.has_metric() &&
                            result.problem.has_state_dependent_costs();
    if (need_sdac_bounds && result.fixpoint_rpg) {
        int final_layer = static_cast<int>(result.fixpoint_rpg->get_layer_count()) - 1;
        const auto& actions = result.problem.actions();
        result.sdac_cost_lower_bounds.reserve(actions.size());
        for (size_t i = 0; i < actions.size(); ++i) {
            auto interval = result.fixpoint_rpg->evaluate_interval(
                actions[i].cost_id(), final_layer);
            result.sdac_cost_lower_bounds.push_back(std::max(0.0, interval.lower));
        }
    }

    // Store all results
    result.achievers = std::move(final_achievers);
    result.tightened_bounds = std::move(final_bounds);
    result.conservation_laws = std::move(final_conservations);

    // Log per-fluent bounds at verbose level
    if (config.is_verbose() && !result.tightened_bounds.empty()) {
        const auto& pool = result.problem.pool();
        for (const auto& [eid, interval] : result.tightened_bounds) {
            if (!result.problem.is_numeric_type(eid)) continue;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3);
            oss << "[NumericBounds] " << pool.to_string(eid)
                << " = [" << interval.lower << ", " << interval.upper << "]";
            Logger::instance().verbose(oss.str());
        }
    }

    // Record stats
    size_t final_actions = result.problem.actions().size();
    size_t total_removed = initial_actions - final_actions;

    stats.set("action_pruning.initial_actions", static_cast<double>(initial_actions));
    stats.set("action_pruning.final_actions", static_cast<double>(final_actions));
    stats.set("action_pruning.removed_actions", static_cast<double>(total_removed));
    stats.set("action_pruning.iterations", static_cast<double>(iteration));

    Logger::instance().component(VerbosityLevel::INFO, "ActionPruning", {
        {"total removed", std::to_string(total_removed) + "/" + std::to_string(initial_actions)},
        {"iterations", std::to_string(iteration)},
        {"time", std::to_string(pass_timer.elapsed_ms()) + "ms"}
    });
}

} // namespace rantanplan
