#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Replaces static fluents with their constant initial-state values.
 *
 * A static fluent is one that no action effect writes to. After CWA, every
 * such fluent has a known constant value. This pass:
 *   1. Identifies static fluents (not in any effect LHS)
 *   2. Substitutes them with constants in all expressions
 *   3. Folds resulting constant sub-expressions (e.g., (* 6.0 2.0) → 12.0)
 *   4. Removes static fluents from initial state and grounded fluent list
 *
 * Must run after CWA (needs complete initial state) and before RPG
 * (so RPG sees simplified expressions and computes tighter bounds).
 */
class StaticFluentPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "StaticFluent"; }
};

} // namespace rantanplan
