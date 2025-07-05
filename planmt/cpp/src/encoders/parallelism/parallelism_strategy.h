#pragma once

#include "../../problem/problem.h"
#include "../z3_variable_factory.h"
#include <z3++.h>
#include <memory>
#include <string>

namespace planmt {

/**
 * @brief Abstract base class for different parallelism encoding strategies
 * 
 * This class defines the interface for encoding parallelism constraints in planning problems.
 * Different concrete implementations provide various semantics for action execution.
 */
class ParallelismStrategy {
public:
    virtual ~ParallelismStrategy() = default;
    
    /**
     * @brief Encode parallelism constraints for a given timestep
     * @param timestep The timestep for which to encode parallelism
     * @return A Z3 expression representing the parallelism constraints
     */
    virtual std::shared_ptr<z3::expr> encode_parallelism(int timestep) = 0;
    
    /**
     * @brief Initialize the strategy with problem context
     * @param problem The planning problem instance
     * @param ctx Z3 context for creating expressions
     * @param var_factory Factory for creating Z3 variables
     */
    virtual void initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) = 0;
    
    /**
     * @brief Get the name of this parallelism strategy
     * @return String identifier for this strategy
     */
    virtual std::string get_name() const = 0;
};

} // namespace planmt
