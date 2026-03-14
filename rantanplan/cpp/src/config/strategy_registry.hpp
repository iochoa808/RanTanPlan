#pragma once

#include "strategy_spec.hpp"
#include <map>
#include <vector>
#include <string>

namespace rantanplan {

class StrategyRegistry {
public:
    /// Look up a strategy spec by name.
    /// Throws std::invalid_argument if the name is unknown.
    static const StrategySpec& get(const std::string& name);

    static bool exists(const std::string& name);

    static std::vector<std::string> list_strategies();

private:
    static const std::map<std::string, StrategySpec>& get_registry();
};

} // namespace rantanplan
