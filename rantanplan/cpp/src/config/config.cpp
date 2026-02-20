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

    // Validate horizon schedule name (catches typos before search begins)
    const auto& sched = planner.horizon_schedule;
    if (sched != "linear" && sched != "arithmetic" && sched != "geometric" && sched != "doubling") {
        throw std::invalid_argument(
            "Unknown horizon schedule: '" + sched +
            "'. Valid values: linear, arithmetic, geometric, doubling");
    }

    // Non-linear horizon schedules rely on prefix-monotone front-loading, which
    // is only implemented in SequentialPlanner.  DoubleTailPlanner tests a
    // specific (forward_end, backward_start) pair per iteration and cannot skip
    // horizons while preserving completeness.
    if (planner.horizon_schedule != "linear" &&
        StrategyRegistry::create(planner.strategy)->uses_double_tail()) {
        throw std::invalid_argument(
            "Non-linear horizon schedule '" + planner.horizon_schedule +
            "' is not compatible with double-tail strategy '" + planner.strategy + "'. "
            "Use a non-dt strategy or --horizon-schedule linear.");
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