#pragma once

#include <string>
#include <vector>
#include "../problem/problem.hpp"
#include "../symmetries/smt_symmetry_checker.hpp"

namespace rantanplan {

/**
 * @brief Accumulated state threaded through a preprocessing pipeline.
 *
 * Each pass reads and updates this struct. The pipeline short-circuits
 * if any pass sets proven_unsolvable to true.
 */
struct PipelineResult {
    Problem problem;                    ///< Current problem (transformed by each pass)
    bool proven_unsolvable = false;     ///< True if any pass proved unsolvability
    std::string unsolvable_reason;      ///< Name of the pass that proved unsolvability
    int lower_bound = 0;                ///< Max lower bound across all passes so far
    std::vector<ObjectSwap> detected_object_swaps; ///< From SymmetryDetectionPass (pre-grounding)
    std::vector<SymmetryInfo> symmetry_data; ///< From SymmetryCompletionPass (post-CWA)
};

/**
 * @brief Abstract base class for preprocessing passes.
 *
 * A pass reads and updates a PipelineResult in place. It may transform
 * the problem, set a lower bound, or prove unsolvability.
 *
 * Passes are stateless — all state flows through PipelineResult.
 */
class Pass {
public:
    /// Read and update the pipeline result. Called exactly once per pipeline run.
    virtual void apply(PipelineResult& result) const = 0;

    /// Human-readable name for logging and diagnostics.
    virtual std::string name() const = 0;

    virtual ~Pass() = default;
};

} // namespace rantanplan
