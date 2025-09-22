#include "cli_parser.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

namespace planmt {

// Parse command line arguments to configure the planner
// This parser handles configuration options for the C++ backend
// Skip first two arguments which are input/output filenames
void CLIParser::parse(Config& config, int argc, char* argv[]) {
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Verbosity options
        if (arg == "--verbosity") {
            if (i + 1 < argc) {
                config.global.verbosity = parse_verbosity(argv[++i]);
            } else {
                throw std::invalid_argument("--verbosity requires a value (silent, info, verbose, debug)");
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
        // Planner options
        else if (arg == "--parallelism") {
            if (i + 1 < argc) {
                config.planner.parallelism_strategy = argv[++i];
            } else {
                throw std::invalid_argument("--parallelism requires a value");
            }
        }
        else if (arg == "--encoder") {
            if (i + 1 < argc) {
                config.planner.encoder = argv[++i];
            } else {
                throw std::invalid_argument("--encoder requires a value");
            }
        }
        else if (arg == "--max-steps") {
            if (i + 1 < argc) {
                config.planner.max_steps = std::stoi(argv[++i]);
            } else {
                throw std::invalid_argument("--max-steps requires a value");
            }
        }
        // Propagator options
        else if (arg == "--propagator") {
            if (i + 1 < argc) {
                config.propagators.type = argv[++i];
            } else {
                throw std::invalid_argument("--propagator requires a value");
            }
        }
        else if (arg == "--no-persist-clauses") {
            config.propagators.persist_clauses = false;
        }
        // Interference Analyzer options
        else if (arg == "--lazy-interference") {
            config.interference_analyzer.type = "lazy";
        }
        else if (arg == "--semantic-interference") {
            config.interference_analyzer.type = "semantic";
        }
        // Symmetry options
        else if (arg == "--detect-symmetries") {
            config.symmetry.detect_symmetries = true;
        }
        // Action removal options
        else if (arg == "--no-action-removal") {
            config.global.enable_action_removal = false;
        }
        // Statistics options
        else if (arg == "--stats-file") {
            if (i + 1 < argc) {
                config.global.stats_file = argv[++i];
            } else {
                throw std::invalid_argument("--stats-file requires a filename");
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