#pragma once

#include "../../problem/problem.hpp"
#include "../z3_variable_factory.hpp"
#include "../../analysis/graph.hpp"
#include <z3++.h>
#include <memory>
#include <string>

namespace rantanplan {

// Forward declarations
class InterferenceAnalysis;

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
    virtual void initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) {
        problem_ = &problem;
        ctx_ = &ctx;
        variable_factory_ = &var_factory;
    }
    
    /**
     * @brief Get the name of this parallelism strategy
     * @return String identifier for this strategy
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Set the interference analyzer for this strategy
     * @param analyzer The interference analyzer to use
     */
    virtual void set_interference_analyzer(std::unique_ptr<InterferenceAnalysis> analyzer) = 0;

    /**
     * @brief Get access to the interference analyzer (if available)
     * @return Pointer to the interference analyzer, or nullptr if not available
     */
    virtual const InterferenceAnalysis* get_interference_analyzer() const = 0;

    /**
     * @brief Check if this strategy allows concurrent action execution
     * @return true if multiple actions can execute at the same timestep (forall/exists semantics),
     *         false if at most one action per timestep (sequential semantics)
     */
    virtual bool allows_concurrent_actions() const = 0;

protected:
    const Problem* problem_ = nullptr;
    z3::context* ctx_ = nullptr;
    Z3VariableFactory* variable_factory_ = nullptr;
};

} // namespace rantanplan
