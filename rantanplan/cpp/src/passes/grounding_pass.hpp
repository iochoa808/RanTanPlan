#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Reachability grounding pass.
 *
 * Runs the delete-relaxation fixpoint grounder on a lifted problem,
 * producing a ground problem where every action is fully instantiated.
 *
 * This pass SUBSUMES the Boolean RPG: it performs the same reachability
 * analysis but also grounds actions simultaneously.  When this pass runs,
 * the BooleanRPGPass should be skipped.
 *
 * Results:
 *   - problem is replaced with the grounded version
 *   - lower_bound is set from the fixpoint layer depth
 *   - proven_unsolvable is set if goal facts are unreachable
 */
class GroundingPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "grounding"; }
};

} // namespace rantanplan
