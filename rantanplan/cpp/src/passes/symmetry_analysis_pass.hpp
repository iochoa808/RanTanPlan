#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Unified symmetry analysis pass (merges detection + completion).
 *
 * Detection: uses the original (pre-grounding) problem snapshot to find
 * symmetric object pairs via SMT equivalence checking.
 *
 * Completion: uses the current (grounded) problem to compute variable
 * pairs and action pairs for each detected swap.
 *
 * The intermediate detected_object_swaps stays local to this pass.
 */
class SymmetryAnalysisPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "symmetry-analysis"; }
};

} // namespace rantanplan
