#pragma once

#include "propagator_strategy.hpp"

namespace rantanplan {

/**
 * @brief Null object pattern implementation for PropagatorStrategy
 *
 * This class provides a no-op implementation of the propagator interface,
 * allowing the sequential planner to operate without propagation when no
 * specific propagator is provided.  When logging is enabled, the base class
 * handles variable registration and decision tracking automatically.
 */
class NullPropagator : public PropagatorStrategy {
public:
    NullPropagator(z3::solver& solver, const BaseEncoder& encoder)
        : PropagatorStrategy(solver, encoder) {}
    ~NullPropagator() override = default;

    void register_timestep_variables(int timestep) override {
        // Base class handles inc logging and fluent/action variable registration
        PropagatorStrategy::register_timestep_variables(timestep);
    }

    void cleanup() override {
        // No-op: null propagator has nothing to clean up
    }

    std::string get_name() const override {
        return "none";
    }
};

} // namespace rantanplan
