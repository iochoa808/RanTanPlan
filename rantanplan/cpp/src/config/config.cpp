#include "config.hpp"
#include "cli_parser.hpp"
#include "strategy_registry.hpp"
#include <stdexcept>

namespace rantanplan {

std::once_flag Config::initialized_flag_;
std::unique_ptr<Config> Config::instance_;

Config& Config::instance() {
    std::call_once(initialized_flag_, []() {
        instance_ = std::unique_ptr<Config>(new Config());
    });
    return *instance_;
}

void Config::initialize(int argc, char* argv[]) {
    CLIParser parser;
    parser.parse(*this, argc, argv);
    validate();
}

void Config::validate() const {
    // Validate strategy exists
    if (!StrategyRegistry::exists(planner.strategy)) {
        throw std::invalid_argument("Unknown strategy: " + planner.strategy +
                                   ". Use --list-strategies to see available options.");
    }

    // Validate global settings
    if (global.timeout <= 0) {
        throw std::invalid_argument("Timeout must be positive");
    }

    if (planner.max_steps <= 0) {
        throw std::invalid_argument("Max steps must be positive");
    }
}

} // namespace rantanplan