#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Closed-World Assumption initial state completion pass.
 *
 * Ensures every grounded fluent has an explicit initial state assignment.
 * Fluents without an assignment get a default value:
 *   - Boolean fluents → false
 *   - Integer fluents → 0
 *   - Real fluents    → 0.0
 *
 * This is necessary when the C++ grounder produces a ground problem,
 * because the initial state is copied from the lifted problem and only
 * contains explicitly stated assignments.  The Python/UP path already
 * fills in defaults, so this pass is a no-op in that case.
 */
class CWAInitialStatePass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "cwa-initial-state"; }
};

} // namespace rantanplan
