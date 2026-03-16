#include "numeric_rpg_pass.hpp"
#include "../analysis/numeric_relaxed_planning_graph.hpp"

#include <z3++.h>
#include <algorithm>
#include <set>

namespace rantanplan {

void NumericRPGPass::apply(PipelineResult& result) const {
    z3::context ctx;
    NumericRelaxedPlanningGraph numeric_rpg(result.problem, ctx);

    // When the problem has SDAC, we need the RPG to reach fixpoint so that
    // interval bounds on cost expressions are as tight as possible.
    bool need_sdac_bounds = result.problem.has_metric() &&
                            result.problem.has_state_dependent_costs();
    if (need_sdac_bounds) {
        numeric_rpg.set_early_termination(false);
    }

    bool goals_reachable = numeric_rpg.build();

    if (!goals_reachable) {
        result.proven_unsolvable = true;
        result.unsolvable_reason = name();
        return;
    }

    result.lower_bound = std::max(result.lower_bound, numeric_rpg.get_minimum_steps_lower_bound());

    auto removed_indices = numeric_rpg.get_removable_action_indices();

    // Compute SDAC cost lower bounds before action removal, filtering out
    // removed actions so the bounds align with the post-removal problem.
    if (need_sdac_bounds) {
        std::set<size_t> removed_set(removed_indices.begin(), removed_indices.end());
        int final_layer = static_cast<int>(numeric_rpg.get_layer_count()) - 1;
        auto& actions = result.problem.actions();

        result.sdac_cost_lower_bounds.reserve(actions.size() - removed_set.size());
        for (size_t i = 0; i < actions.size(); ++i) {
            if (removed_set.count(i)) continue;
            auto interval = numeric_rpg.evaluate_interval(actions[i].cost_id(), final_layer);
            result.sdac_cost_lower_bounds.push_back(std::max(0.0, interval.lower));
        }
    }

    if (!removed_indices.empty()) {
        result.problem = result.problem.without_actions(removed_indices);
    }
}

} // namespace rantanplan
