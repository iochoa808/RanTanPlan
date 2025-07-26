#pragma once

#include "propagator_strategy.h"

namespace planmt {

/**
 * @brief Null object pattern implementation for PropagatorStrategy
 * 
 * This class provides a no-op implementation of the propagator interface,
 * allowing the sequential planner to operate without propagation when no
 * specific propagator is provided.
 */
class NullPropagator : public PropagatorStrategy {
public:
    NullPropagator() = default;
    ~NullPropagator() override = default;
    
    void initialize(z3::solver& solver, const GroundedEncoder& encoder) override {
        // No-op: null propagator does nothing
    }
    
    void register_timestep_variables(int timestep) override {
        // No-op: null propagator doesn't register variables
    }
    
    void cleanup() override {
        // No-op: null propagator has nothing to clean up
    }
    
    std::string get_name() const override {
        return "none";
    }
    
    PropagatorType get_type() const override {
        return PropagatorType::NULL_PROPAGATOR;
    }
};

} // namespace planmt
