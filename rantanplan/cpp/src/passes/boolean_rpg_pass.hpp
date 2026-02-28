#pragma once

#include "pass.hpp"

namespace rantanplan {

/**
 * @brief Uses the Boolean Relaxed Planning Graph to check goal reachability,
 *        compute a lower bound on plan length, and remove unreachable actions.
 *
 * No external dependencies — RelaxedPlanningGraph only needs const Problem&.
 */
class BooleanRPGPass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "boolean-rpg"; }
};

} // namespace rantanplan
