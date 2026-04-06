#pragma once

#include "strategy_spec.hpp"
#include <map>
#include <vector>
#include <string>

namespace rantanplan {

struct StrategyEntry {
    StrategySpec spec;
    std::string description;
};

class StrategyRegistry {
public:
    /// Look up a strategy spec by name.
    /// Throws std::invalid_argument if the name is unknown.
    static const StrategySpec& get(const std::string& name);

    static bool exists(const std::string& name);

    static std::vector<std::string> list_strategies();

    /// Return the full registry (name -> entry with spec + description).
    static const std::map<std::string, StrategyEntry>& get_registry();
};

} // namespace rantanplan
