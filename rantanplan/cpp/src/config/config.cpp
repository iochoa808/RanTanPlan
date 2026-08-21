#include "config.hpp"
#include "cli_parser.hpp"
#include "strategy_registry.hpp"
#include "strategy_factory.hpp"
#include "strategy_spec.hpp"
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

    // [XTS] Validate array/set frame-axiom mode name (catches typos before search begins).
    const auto& afm = global.array_frame_mode;
    if (afm != "disequality" && afm != "ite") {
        throw std::invalid_argument(
            "Unknown array-frame-mode: '" + afm + "'. Valid values: disequality, ite");
    }

    // [XTS-UnFun] Validate array/set encoding backend name (catches typos before search begins).
    const auto& aenc = global.array_encoding;
    if (aenc != "theory" && aenc != "uf") {
        throw std::invalid_argument(
            "Unknown array-encoding: '" + aenc + "'. Valid values: theory, uf");
    }

    // Validate search mode and resolve planner kind (may throw if mode
    // is incompatible with the strategy, e.g. optimal + double-tail).
    auto spec = StrategyRegistry::get(planner.strategy);

    // [XTS-UnFun] UF array encoding is only implemented by the plain grounded
    // encoder's effect paths; Chained/R2E would emit Theory store equations that
    // never link to the UF frame axioms (silently unsound). Reject here so the
    // error surfaces as a "Configuration error" instead of a late terminate
    // (StrategyFactory::create_encoder keeps the same check as a backstop).
    // Only an explicit --array-encoding uf is an error here: "uf" is also the
    // default, and a chained/R2E strategy must stay usable without the caller
    // having to opt out of a default they never chose. In that case
    // StrategyFactory::create_encoder falls back to Theory and logs it.
    if (aenc == "uf" && global.array_encoding_explicit &&
        spec.encoder != EncoderFamily::Grounded) {
        throw std::invalid_argument(
            "--array-encoding uf is only supported by the grounded encoder family; "
            "strategy '" + planner.strategy + "' uses a chained/R2E encoder. "
            "Use a grounded-encoder strategy or --array-encoding theory.");
    }

    SearchMode mode = parse_search_mode(planner.mode);
    spec.planner = resolve_planner_kind(mode, spec);

    // Strategy compatibility validation (single point of responsibility
    // in StrategyFactory::validate — covers both spec-internal and
    // spec-vs-config constraints).
    StrategyFactory::validate(spec, planner.strategy, planner.horizon_schedule);

    // Validate global settings (0 = no timeout, any positive value is valid)
    if (global.timeout < 0) {
        throw std::invalid_argument("Timeout must be non-negative (0 = no timeout)");
    }

    if (planner.max_steps <= 0) {
        throw std::invalid_argument("Max steps must be positive");
    }
}

} // namespace rantanplan
