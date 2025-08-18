#pragma once

#include "propagator_strategy.h"
#include "propagator_types.h"
#include "../../problem/problem.h"
#include <z3++.h>
#include <memory>
#include <string>

namespace planmt {

/**
 * @brief Factory for creating propagator strategies
 * 
 * This factory encapsulates the creation of different propagator strategies
 * and provides a clean interface for the planner to obtain strategy instances
 * without needing to know about concrete strategy types.
 */
class PropagatorFactory {
public:
    /**
     * @brief Create a propagator strategy instance with solver (for user propagators)
     * @param type The type of strategy to create
     * @param solver Z3 solver for the propagator (enables user propagator callbacks)
     * @param problem The planning problem (required for planning propagators)
     * @param encoder The base encoder providing variable factory access
     * @return A unique pointer to the created strategy
     */
    static std::unique_ptr<PropagatorStrategy> create_strategy(
        PropagatorType type, 
        z3::solver& solver,
        const Problem& problem,
        const BaseEncoder& encoder);

    /**
     * @brief Create a propagator strategy instance from string with solver
     * @param strategy_name The name of the strategy ("null", "forall")
     * @param solver Z3 solver for the propagator (enables user propagator callbacks)
     * @param problem The planning problem (required for planning propagators)
     * @param encoder The base encoder providing variable factory access
     * @return A unique pointer to the created strategy
     */
    static std::unique_ptr<PropagatorStrategy> create_strategy(
        const std::string& strategy_name,
        z3::solver& solver,
        const Problem& problem,
        const BaseEncoder& encoder);

    /**
     * @brief Get the string name for a propagator type
     * @param type The propagator type
     * @return String representation of the type
     */
    static std::string get_strategy_name(PropagatorType type);

    /**
     * @brief Parse a string into a propagator type
     * @param strategy_name The strategy name to parse
     * @return The corresponding propagator type, or NULL_PROPAGATOR if invalid
     */
    static PropagatorType parse_strategy_type(const std::string& strategy_name);

    /**
     * @brief Get list of available propagator types
     * @return Vector of available type strings
     */
    static std::vector<std::string> get_available_types();
};

} // namespace planmt
