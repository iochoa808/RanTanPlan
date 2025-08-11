#pragma once

#include "../problem/problem.h"
#include "../problem/plan.h"
#include "../encoders/base_encoder.h"
#include "propagators/propagator_strategy.h"
#include "propagators/propagator_factory.h"
#include <z3++.h>
#include <memory>

// This class is able to search for a plan in a sequential manner by using an encoder.
namespace planmt {

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
    void set_propagator_strategy(PropagatorType type);
    void set_propagator_strategy(const std::string& strategy_name);
    std::string get_propagator_strategy_name() const;
    PropagatorType get_propagator_type() const;

private:
    const Problem& problem_;
    BaseEncoder& encoder_;
    z3::context& ctx_;
    z3::solver solver_;  // Solver to maintain current constraints state
    bool solution_found_ = false;  // Track if last search found a solution
    
    // Propagator strategy for custom propagation (defaults to null propagator)
    std::unique_ptr<PropagatorStrategy> propagator_strategy_;
    
    // Extract plan from Z3 model
    Plan extract_plan(const z3::model& model, int max_timestep);
    
    // Helper methods for parallel plan extraction
    std::vector<const Action*> extract_parallel_actions_at_timestep(const z3::model& model, int timestep);
    std::vector<const Action*> topologically_sort_actions(const std::vector<const Action*>& actions);
    
    // Debug method to output constraints for a given timestep
    void debug_output_constraints();
};

} // namespace planmt
