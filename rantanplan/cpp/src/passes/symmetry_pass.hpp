#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Detects symmetric object pairs using SMT-based checking.
 *
 * Runs BEFORE grounding. Only does the expensive SMT equivalence check.
 * Stores detected ObjectSwaps in PipelineResult::detected_object_swaps.
 * Variable/action pairs are computed later by SymmetryCompletionPass.
 */
class SymmetryDetectionPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "symmetry-detection"; }
};

/**
 * @brief Computes variable pairs and action pairs for known symmetric objects.
 *
 * Runs AFTER grounding + CWA. Uses the grounded problem (with complete
 * initial state) to compute variable pairs and action pairs for each
 * ObjectSwap detected by SymmetryDetectionPass.
 */
class SymmetryCompletionPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "symmetry-completion"; }
};

} // namespace rantanplan
