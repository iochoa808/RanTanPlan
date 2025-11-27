#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "propagators/propagator_strategy.hpp"
#include <z3++.h>
#include <memory>
#include <string>

namespace rantanplan {

/**
 * @brief Base interface for all planner implementations
 *
 * This interface abstracts different planning strategies (e.g., sequential single-ended,
 * double-tail/double-ended). The planner type is part of the strategy configuration,
 * allowing strategies to control both the encoding and the search approach.
 */
class BasePlanner {
public:
    virtual ~BasePlanner() = default;

    /**
     * @brief Main search method - runs the planning algorithm
     * @return Plan object (may be empty if no solution found)
     */
    virtual Plan search() = 0;

    /**
     * @brief Check if the last search found a solution
     * @return true if a valid plan was found, false otherwise
     */
    virtual bool solution_found() const = 0;

    /**
     * @brief Set the propagator strategy for this planner
     * @param propagator The propagator strategy to use
     */
    virtual void set_propagator_strategy(std::unique_ptr<PropagatorStrategy> propagator) = 0;

    /**
     * @brief Get the name of the current propagator strategy
     * @return Name of the propagator (e.g., "NullPropagator", "LazyForallPropagator")
     */
    virtual std::string get_propagator_strategy_name() const = 0;

    /**
     * @brief Access to the underlying Z3 solver
     * @return Reference to the Z3 solver used by this planner
     */
    virtual z3::solver& get_solver() = 0;
};

} // namespace rantanplan
