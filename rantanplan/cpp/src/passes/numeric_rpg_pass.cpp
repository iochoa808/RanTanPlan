#include "numeric_rpg_pass.hpp"
#include "../analysis/numeric_relaxed_planning_graph.hpp"

#include <z3++.h>
#include <algorithm>

namespace rantanplan {

void NumericRPGPass::apply(PipelineResult& result) const {
    z3::context ctx;
    NumericRelaxedPlanningGraph numeric_rpg(result.problem, ctx);
    bool goals_reachable = numeric_rpg.build();

    if (!goals_reachable) {
        result.proven_unsolvable = true;
        result.unsolvable_reason = name();
        return;
    }

    result.lower_bound = std::max(result.lower_bound, numeric_rpg.get_minimum_steps_lower_bound());

    auto removed_indices = numeric_rpg.get_removable_action_indices();
    if (!removed_indices.empty()) {
        result.problem = result.problem.without_actions(removed_indices);
    }
}

} // namespace rantanplan
