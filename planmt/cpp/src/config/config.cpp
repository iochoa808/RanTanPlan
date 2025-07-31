#include "config.h"
#include "cli_parser.h"
#include <stdexcept>
#include <memory>
#include <vector>

namespace planmt {

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
    // Validate global settings
    if (global.timeout <= 0) {
        throw std::invalid_argument("Timeout must be positive");
    }
    
    // Validate planner settings
    if (planner.max_steps <= 0) {
        throw std::invalid_argument("Max steps must be positive");
    }
    
    const std::vector<std::string> valid_strategies = {"sequential", "forall", "exists"};
    bool valid_strategy = false;
    for (const auto& strategy : valid_strategies) {
        if (planner.strategy == strategy) {
            valid_strategy = true;
            break;
        }
    }
    if (!valid_strategy) {
        throw std::invalid_argument("Invalid planner strategy: " + planner.strategy);
    }
    
    // Validate propagator settings
    const std::vector<std::string> valid_propagators = {"null", "forall", "exists"};
    bool valid_propagator = false;
    for (const auto& prop : valid_propagators) {
        if (propagators.type == prop) {
            valid_propagator = true;
            break;
        }
    }
    if (!valid_propagator) {
        throw std::invalid_argument("Invalid propagator type: " + propagators.type);
    }
}

} // namespace planmt