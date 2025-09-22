#pragma once

#include "../problem/problem.hpp"
#include "grounded_encoder.hpp"
#include <z3++.h>

namespace planmt {


/**
 * @brief Reified grounded encoder for planning problems
 * 
 * TODO: Implement reification logic for CNF preconditions and goals.
 */
class ReifiedGroundedEncoder : public GroundedEncoder {
public:
    // Constructor
    ReifiedGroundedEncoder(const Problem& problem, z3::context& ctx);

    // TODO: Override encoding methods to add reification
    std::shared_ptr<z3::expr> encode_actions(int t) override;
    std::shared_ptr<z3::expr> encode_goal(int t) override;
    
private:
    // TODO: Add reification data structures and methods
};

} // namespace planmt
