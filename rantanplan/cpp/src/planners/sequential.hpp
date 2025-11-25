#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "../encoders/base_encoder.hpp"
#include "propagators/propagator_strategy.hpp"
#include <z3++.h>
#include <memory>

// This class is able to search for a plan in a sequential manner by using an encoder.
namespace rantanplan {

class SequentialPlanner {
public:
    // Constructor
    SequentialPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx);

    // Constructor with propagator support
    SequentialPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx,
                     std::unique_ptr<PropagatorStrategy> propagator);

    // Search for a plan and return it
    Plan search();
    
    // Check if the last search found a solution (even if empty plan)
    bool solution_found() const { return solution_found_; }
    
    // Strategy management
    void set_propagator_strategy(std::unique_ptr<PropagatorStrategy> propagator);
    std::string get_propagator_strategy_name() const;

    // Get solver reference (for creating propagators)
    z3::solver& get_solver() { return solver_; }

private:
    const Problem& problem_;
    BaseEncoder& encoder_;
    z3::context& ctx_;
    z3::solver solver_;  // Solver to maintain current constraints state
    bool solution_found_ = false;  // Track if last search found a solution

    // Propagator strategy for custom propagation (defaults to null propagator)
    std::unique_ptr<PropagatorStrategy> propagator_strategy_;
    
    // Debug method to output constraints for a given timestep
    void debug_output_constraints();

    // Collect comprehensive statistics
    void collect_statistics();

    // Add constraints for a given timestep (actions, frames, symmetries, parallelism)
    void add_timestep_constraints(int timestep);
};

} // namespace rantanplan
