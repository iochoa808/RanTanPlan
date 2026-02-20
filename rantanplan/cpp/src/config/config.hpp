#pragma once

#include <string>
#include <mutex>
#include <memory>

namespace rantanplan {

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
        // IMPORTANT: Early termination is UNSOUND for action removal with Numeric RPG!
        // The relaxation uses interval bounds [lower, upper] which may contain values unreachable
        // by actual execution. For example: if x=200 and we add 100, bounds become [200, 300],
        // but intermediate values like 201 may be unreachable. This means actions applicable
        // in later RPG layers (due to over-approximated bounds) might not be truly reachable,
        // but we still need to discover them to avoid incorrect pruning. Therefore, we must
        // build to fixpoint rather than stopping early when goals become achievable.
        bool rpg_early_termination = false;  // Stop RPG construction when goals are reachable (DEFAULT: false - required for sound action removal)
        bool use_numeric_rpg = false;  // Use NumericRelaxedPlanningGraph (true) or RelaxedPlanningGraph (false) for action removal
        bool compare_rpgs = false;  // Run RPG comparison tool to debug action removal differences
    } global;
    
    struct Planner {
        std::string strategy = "seq";  // Strategy name (replaces parallelism_strategy, encoder, propagator, interference)
        int max_steps = 500;
        int start_timestep = 0;  // Starting timestep for search (can be set by RPG lower bound)
        std::string horizon_schedule = "linear";  // Schedule type: linear|arithmetic|geometric|doubling
    } planner;
    
    struct Symmetry {
        bool detect_symmetries = false;  // Enable symmetry detection
    } symmetry;

    struct FormulaExport {
        bool export_formula = false;      // Enable formula export mode
        int timestep = -1;                // Timestep to export formula for
        std::string output_file = "";     // Output file for formula (required)
    } formula_export;

    struct Logging {
        std::string log_file = "";        // Empty = no logging; path = solver decision log
    } logging;

    void initialize(int argc, char* argv[]);
    void validate() const;

    bool is_silent() const { return global.verbosity == VerbosityLevel::SILENT; }
    bool is_info() const { return global.verbosity >= VerbosityLevel::INFO; }
    bool is_verbose() const { return global.verbosity >= VerbosityLevel::VERBOSE; }
    bool is_debug() const { return global.verbosity >= VerbosityLevel::DEBUG; }
    bool has_log_file() const { return !logging.log_file.empty(); }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    static std::once_flag initialized_flag_;
    static std::unique_ptr<Config> instance_;
    
    friend std::default_delete<Config>;
};

} // namespace rantanplan