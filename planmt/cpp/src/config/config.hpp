#pragma once

#include <string>
#include <mutex>
#include <memory>

namespace planmt {

enum class VerbosityLevel {
    SILENT = 0,
    INFO = 1, 
    VERBOSE = 2,
    DEBUG = 3
};

class Config {
public:
    static Config& instance();
    
    struct Global {
        VerbosityLevel verbosity = VerbosityLevel::INFO;
        bool debug_mode = false;
        int timeout = 3600;
        std::string log_level = "INFO";
        std::string stats_file = "";  // Empty means no file output
        bool enable_action_removal = true;  // Enable RPG-based action removal optimization
        bool persist_clauses = true;  // Z3 persist clauses setting for user propagators
        double epsilon = 1e-6;  // Numerical tolerance for floating-point comparisons
        bool rpg_early_termination = true;  // Stop RPG construction when goals are reachable (sound for satisficing, may be unsound for optimal planning)
        bool use_numeric_rpg = true;  // Use NumericRelaxedPlanningGraph (true) or RelaxedPlanningGraph (false) for action removal
    } global;
    
    struct Planner {
        std::string strategy = "seq";  // Strategy name (replaces parallelism_strategy, encoder, propagator, interference)
        int max_steps = 100;
        int start_timestep = 0;  // Starting timestep for search (can be set by RPG lower bound)
    } planner;
    
    struct Symmetry {
        bool detect_symmetries = false;  // Enable symmetry detection
    } symmetry;

    struct FormulaExport {
        bool export_formula = false;      // Enable formula export mode
        int timestep = -1;                // Timestep to export formula for
        std::string output_file = "";     // Output file for formula (required)
    } formula_export;
    
    void initialize(int argc, char* argv[]);
    void validate() const;
    
    bool is_silent() const { return global.verbosity == VerbosityLevel::SILENT; }
    bool is_info() const { return global.verbosity >= VerbosityLevel::INFO; }
    bool is_verbose() const { return global.verbosity >= VerbosityLevel::VERBOSE; }
    bool is_debug() const { return global.verbosity >= VerbosityLevel::DEBUG; }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    static std::once_flag initialized_flag_;
    static std::unique_ptr<Config> instance_;
    
    friend std::default_delete<Config>;
};

} // namespace planmt