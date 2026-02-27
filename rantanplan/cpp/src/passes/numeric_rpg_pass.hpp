#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Uses the Numeric Relaxed Planning Graph (SMT-based) to check goal
 *        reachability, compute a lower bound on plan length, and remove
 *        unreachable actions.
 *
 * Creates a local z3::context internally — no external dependencies.
 */
class NumericRPGPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "numeric-rpg"; }
};

} // namespace rantanplan
