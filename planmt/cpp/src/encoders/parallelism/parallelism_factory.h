#pragma once

#include "parallelism_strategy.h"
#include <memory>
#include <string>

namespace planmt {

/**
 * @brief Factory for creating parallelism strategies
 * 
 * This factory encapsulates the creation of different parallelism strategies
 * and provides a clean interface for the encoder to obtain strategy instances
 * without needing to know about concrete strategy types.
 */
class ParallelismFactory {
public:
    /**
     * @brief Enum for selecting parallelism strategy types
     */
    enum class ParallelismType {
        SEQUENTIAL,  // Exactly one action per timestep (default)
        FORALL,      // Set of actions in a timestep can execute in parallel if they don't conflict
        EXISTS       // Set of actions in a timestep can execute if there exists
                     //    at least one order where actions do not interfere with actions further
                     //    down the line.
    };

    /**
     * @brief Create a parallelism strategy instance
     * @param type The type of strategy to create
     * @return A unique pointer to the created strategy
     */
    static std::unique_ptr<ParallelismStrategy> create_strategy(ParallelismType type);

    /**
     * @brief Create a parallelism strategy instance from string
     * @param strategy_name The name of the strategy ("sequential", "forall", "exists")
     * @return A unique pointer to the created strategy
     */
    static std::unique_ptr<ParallelismStrategy> create_strategy(const std::string& strategy_name);

    /**
     * @brief Get the string name for a parallelism type
     * @param type The parallelism type
     * @return String representation of the type
     */
    static std::string get_strategy_name(ParallelismType type);

    /**
     * @brief Parse a string into a parallelism type
     * @param strategy_name The strategy name to parse
     * @return The corresponding parallelism type, or SEQUENTIAL if invalid
     */
    static ParallelismType parse_strategy_type(const std::string& strategy_name);
};

} // namespace planmt
