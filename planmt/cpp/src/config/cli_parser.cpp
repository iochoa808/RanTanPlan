#include "cli_parser.hpp"
#include "strategy_registry.hpp"
#include <iostream>
#include <string>
#include <stdexcept>
#include <iomanip>

namespace planmt {

void CLIParser::parse(Config& config, int argc, char* argv[]) {
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
        // Action removal options
        else if (arg == "--no-action-removal") {
            config.global.enable_action_removal = false;
        }
        else if (arg == "--boolean-rpg") {
            config.global.use_numeric_rpg = false;
        }
        else if (arg == "--numeric-rpg") {
            config.global.use_numeric_rpg = true;
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


} // namespace planmt