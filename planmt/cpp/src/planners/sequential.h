#pragma once

#include "../problem/problem.h"
#include "../problem/plan.h"
#include "../encoders/grounded_encoder.h"
#include <z3++.h>

// This class is able to search for a plan in a sequential manner by using an encoder.
namespace planmt {

class SequentialPlanner {
public:
    // Constructor
    SequentialPlanner(const Problem& problem, GroundedEncoder& encoder, z3::context& ctx);

    // Search for a plan and return it
    Plan search();

private:
    const Problem& problem_;
    GroundedEncoder& encoder_;
    z3::context& ctx_;
    z3::solver solver_;  // Solver to maintain current constraints state
    
    // Extract plan from Z3 model
    Plan extract_plan(const z3::model& model, int max_timestep);
    
    // Debug method to output constraints for a given timestep
    void debug_output_constraints();
};

} // namespace planmt
