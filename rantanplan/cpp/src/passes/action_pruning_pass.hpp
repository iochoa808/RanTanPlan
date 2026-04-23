#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Unified action pruning pass (merges NumericRPG + GoalRelevance).
 *
 * Runs a fixpoint loop that alternates forward pruning (RPG-unreachable
 * actions) and backward pruning (goal-irrelevant actions via achiever
 * analysis). The RPG always runs to fixpoint for tightest possible bounds.
 *
 * Produces:
 *   - fixpoint_rpg: final RPG on the pruned problem
 *   - tightened_bounds: numeric bounds (RPG + syntactic + conservation-derived)
 *   - conservation_laws: discovered f + g == C invariants
 *   - achievers: goal-relevant achiever analysis (for PDLA planners)
 *   - sdac_cost_lower_bounds: cost bounds on the FINAL action set
 *
 * Runs when any of rpg.enabled, rpg.goal_relevance, or
 * (local_reasoning.enabled && local_reasoning.rpg_bounds) is set.
 */
class ActionPruningPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "action-pruning"; }
};

} // namespace rantanplan
