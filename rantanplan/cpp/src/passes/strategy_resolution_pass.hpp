#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Resolves and adjusts the strategy spec for the current problem.
 *
 * Reads the strategy name and search mode from Config, looks up the base
 * StrategySpec from the registry, resolves the planner kind for the search
 * mode, and applies problem-specific adjustments (e.g. SDAC + exists-step
 * downgrade).  Stores the final spec in PipelineResult::resolved_spec.
 *
 * Must run before any pass that needs the resolved spec (e.g. InterferencePass).
 */
class StrategyResolutionPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "strategy-resolution"; }
};

} // namespace rantanplan
