#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Detects object symmetries using SMT-based checking and stores
 *        them in PipelineResult for use by the encoder.
 *
 * Creates a temporary Z3 context for detection, then discards it.
 * The detected SymmetryInfo structs (variable pairs, action pairs)
 * are self-contained and don't require Z3 at encoding time.
 */
class SymmetryPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "symmetries"; }
};

} // namespace rantanplan
