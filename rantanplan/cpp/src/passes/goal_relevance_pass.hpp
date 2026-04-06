#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Removes goal-irrelevant actions using semantic achiever analysis.
 *
 * Runs a fixpoint loop:
 *   1. Build NumericRPG → extract bounds + layer data
 *   2. Run AchieversAnalysis (using RPG data) → compute goal-relevant set
 *   3. Remove non-relevant actions from problem
 *   4. Repeat until no more actions removed (or iteration cap reached)
 *
 * The final AchieversAnalysis is stored in PipelineResult for reuse by
 * PDLA planners (avoiding a redundant RPG + achiever computation).
 *
 * Runs when rpg.goal_relevance config is enabled (any strategy).
 * The AchieversAnalysis is only consumed by PDLA planners (via uses_pdla()).
 *
 * SDAC cost expression fluents are included in the relevance closure
 * only when the search mode is optimal or anytime (where cost matters
 * for plan quality). In satisficing mode, cost-only actions are safely
 * removed since any valid plan suffices.
 */
class GoalRelevancePass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "goal-relevance"; }

    // Max iterations read from Config::RPG::goal_relevance_max_iterations
};

} // namespace rantanplan
