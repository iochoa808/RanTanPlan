#include "cli_parser.hpp"
#include "strategy_registry.hpp"
#include <iostream>
#include <string>
#include <stdexcept>
#include <iomanip>

namespace rantanplan {

void CLIParser::parse(Config& config, int argc, char* argv[]) {
    // Track whether --no-action-removal was given so it can override
    // --up-grounding / --boolean-rpg / --numeric-rpg regardless of flag order.
    bool explicit_no_action_removal = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        // Strategy selection
        if (arg == "--strategy") {
            if (i + 1 < argc) {
                config.planner.strategy = argv[++i];
            } else {
                throw std::invalid_argument("--strategy requires a value");
            }
        }
        else if (arg == "--mode") {
            if (i + 1 < argc) {
                config.planner.mode = argv[++i];
            } else {
                throw std::invalid_argument("--mode requires a value (satisficing, optimal, anytime)");
            }
        }
        else if (arg == "--output-plan") {
            if (i + 1 < argc) {
                config.planner.output_plan = argv[++i];
            } else {
                throw std::invalid_argument("--output-plan requires a filename");
            }
        }
        else if (arg == "--list-strategies") {
            // Print available strategies and exit
            std::cout << "Available strategies:" << std::endl;
            auto strategies = StrategyRegistry::list_strategies();
            for (const auto& name : strategies) {
                std::cout << "  " << name << std::endl;
            }
            exit(0);
        }
        // Verbosity options
        else if (arg == "--verbosity") {
            if (i + 1 < argc) {
                config.global.verbosity = parse_verbosity(argv[++i]);
            } else {
                throw std::invalid_argument("--verbosity requires a value");
            }
        }
        else if (arg == "-v") {
            config.global.verbosity = VerbosityLevel::VERBOSE;
        }
        else if (arg == "-vv") {
            config.global.verbosity = VerbosityLevel::DEBUG;
        }
        else if (arg == "--silent") {
            config.global.verbosity = VerbosityLevel::SILENT;
        }
        else if (arg == "--debug") {
            config.global.debug_mode = true;
            config.global.verbosity = VerbosityLevel::DEBUG;
        }
        // Global options
        else if (arg == "--timeout") {
            if (i + 1 < argc) {
                config.global.timeout = std::stoi(argv[++i]);
            } else {
                throw std::invalid_argument("--timeout requires a value");
            }
        }
        else if (arg == "--max-steps") {
            if (i + 1 < argc) {
                config.planner.max_steps = std::stoi(argv[++i]);
            } else {
                throw std::invalid_argument("--max-steps requires a value");
            }
        }
        else if (arg == "--horizon-schedule") {
            if (i + 1 < argc) {
                config.planner.horizon_schedule = argv[++i];
            } else {
                throw std::invalid_argument("--horizon-schedule requires a value (linear|arithmetic|geometric|doubling)");
            }
        }
        // Action removal / grounding options
        else if (arg == "--up-grounding") {
            // Use Unified Planning grounding (Python side) instead of C++ grounding.
            // This disables C++ grounding and enables RPG-based action removal.
            config.global.reachability_grounding = false;
            config.global.enable_action_removal = true;
        }
        else if (arg == "--no-numeric-grounding") {
            config.global.numeric_grounding = false;
        }
        else if (arg == "--no-action-removal") {
            config.global.enable_action_removal = false;
            explicit_no_action_removal = true;
        }
        else if (arg == "--boolean-rpg") {
            config.global.use_numeric_rpg = false;
            config.global.enable_action_removal = true;
        }
        else if (arg == "--numeric-rpg") {
            config.global.use_numeric_rpg = true;
            config.global.enable_action_removal = true;
        }
        // Z3 solver options
        else if (arg == "--no-persist-clauses") {
            config.global.persist_clauses = false;
        }
        // Statistics options
        else if (arg == "--stats-file") {
            if (i + 1 < argc) {
                config.global.stats_file = argv[++i];
            } else {
                throw std::invalid_argument("--stats-file requires a filename");
            }
        }
        // Formula export options
        else if (arg == "--export-formula") {
            config.formula_export.export_formula = true;
        }
        else if (arg == "--formula-timestep") {
            if (i + 1 < argc) {
                config.formula_export.timestep = std::stoi(argv[++i]);
            } else {
                throw std::invalid_argument("--formula-timestep requires a value");
            }
        }
        else if (arg == "--formula-output") {
            if (i + 1 < argc) {
                config.formula_export.output_file = argv[++i];
            } else {
                throw std::invalid_argument("--formula-output requires a filename");
            }
        }
        // Symmetry options
        else if (arg == "--symmetries") {
            config.symmetry.enable_symmetries = true;
        }
        // Logging options
        else if (arg == "--log-file") {
            if (i + 1 < argc) {
                config.logging.log_file = argv[++i];
            } else {
                throw std::invalid_argument("--log-file requires a filename");
            }
        }
    }

    // --no-action-removal takes precedence over any flag that enables it.
    if (explicit_no_action_removal) {
        config.global.enable_action_removal = false;
    }
}

VerbosityLevel CLIParser::parse_verbosity(const std::string& value) const {
    if (value == "silent") return VerbosityLevel::SILENT;
    if (value == "info") return VerbosityLevel::INFO;
    if (value == "verbose") return VerbosityLevel::VERBOSE;
    if (value == "debug") return VerbosityLevel::DEBUG;
    
    throw std::invalid_argument("Invalid verbosity level: " + value + 
                               ". Valid values are: silent, info, verbose, debug");
}


} // namespace rantanplan