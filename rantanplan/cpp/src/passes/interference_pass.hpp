#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Creates the interference analyzer based on the resolved strategy spec.
 *
 * Reads resolved_spec from PipelineResult and constructs the appropriate
 * InterferenceAnalysis variant (eager/lazy, syntactic/semantic).  The analyzer
 * is stored as a live object in PipelineResult::interference — eager variants
 * pre-compute during this pass, lazy variants defer computation to solve time.
 *
 * Must run after all problem-transforming passes (grounding, CWA, RPG) so the
 * analyzer sees the final problem, and after StrategyResolutionPass.
 */
class InterferencePass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "interference"; }
};

} // namespace rantanplan
