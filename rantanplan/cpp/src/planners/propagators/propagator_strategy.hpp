#pragma once

#include "../../encoders/base_encoder.hpp"
#include <z3++.h>
#include <memory>
#include <string>

namespace rantanplan {

// Forward declarations
class BaseEncoder;

/**
 * @brief Abstract base class for different propagator strategies
 * 
 * This class defines the interface for integrating user propagators with the planning process.
 * Different concrete implementations can provide various propagation strategies for planning problems.
 */
class PropagatorStrategy {
public:
    virtual ~PropagatorStrategy() = default;
    
    /**
     * @brief Register variables for a specific timestep after constraints are added
     * @param timestep The timestep for which to register variables
     */
    virtual void register_timestep_variables(int timestep) = 0;
    
    /**
     * @brief Clean up any resources (called automatically via RAII)
     */
    virtual void cleanup() = 0;
    
    /**
     * @brief Get the name of this propagator strategy
     * @return String identifier for this strategy
     */
    virtual std::string get_name() const = 0;

    /**
     * @brief Check if this propagator manages parallelism constraints internally
     * @return true if propagator handles parallelism constraints, false otherwise
     */
    virtual bool manages_parallelism_constraints() const { return false; }
};

} // namespace rantanplan
