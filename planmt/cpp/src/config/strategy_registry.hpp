#pragma once

#include "strategy_configuration.hpp"
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <string>

namespace planmt {

/**
 * @brief Registry for all available strategy configurations
 */
class StrategyRegistry {
public:
    using StrategyFactory = std::function<std::unique_ptr<StrategyConfiguration>()>;

    /**
     * @brief Create a strategy by name
     * @throws std::invalid_argument if strategy name is unknown
     */
    static std::unique_ptr<StrategyConfiguration> create(const std::string& name);

    /**
     * @brief Check if a strategy exists
     */
    static bool exists(const std::string& name);

    /**
     * @brief Get list of all available strategy names
     */
    static std::vector<std::string> list_strategies();

private:
    static std::map<std::string, StrategyFactory>& get_registry();
    static void initialize_builtin_strategies(std::map<std::string, StrategyFactory>& registry);
};

} // namespace planmt
