#include "boolean_rpg_pass.hpp"
#include "../analysis/relaxed_planning_graph.hpp"

#include <algorithm>

namespace rantanplan {

void BooleanRPGPass::apply(PipelineResult& result) const {
    RelaxedPlanningGraph rpg(result.problem);
    bool goals_reachable = rpg.build();

    if (!goals_reachable) {
        result.proven_unsolvable = true;
        result.unsolvable_reason = name();
        return;
    }

    result.lower_bound = std::max(result.lower_bound, rpg.get_minimum_steps_lower_bound());

    auto removed_indices = rpg.get_removable_action_indices();
    if (!removed_indices.empty()) {
        result.problem = result.problem.without_actions(removed_indices);
    }
}

} // namespace rantanplan
