#pragma once

#include "../../encoders/grounded_encoder.h"
#include "propagator_types.h"
#include <z3++.h>
#include <memory>
#include <string>

namespace planmt {

// Forward declarations
class GroundedEncoder;

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
     * @brief Initialize the propagator with the solver and encoder
     * @param solver The Z3 solver instance to register with
     * @param encoder The grounded encoder providing variable factory access
     */
    virtual void initialize(z3::solver& solver, const GroundedEncoder& encoder) = 0;
    
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
     * @brief Get the type of this propagator strategy
     * @return PropagatorType enum value for this strategy
     */
    virtual PropagatorType get_type() const = 0;
};

} // namespace planmt
