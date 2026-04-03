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
        int timeout = 0;              // 0 = no timeout (run indefinitely)
        std::string log_level = "INFO";
        std::string stats_file = "";  // Empty means no file output
        bool persist_clauses = true;  // Z3 persist clauses setting for user propagators
        double epsilon = 1e-6;        // Numerical tolerance for floating-point comparisons
        bool reachability_grounding = true;  // --up-grounding disables this
        bool numeric_grounding = true;       // --no-numeric-grounding disables this
    } global;

    /// Numeric Relaxed Planning Graph settings.
    struct RPG {
        bool enabled = true;            ///< Run NumericRPG preprocessing (lower bound + action removal)
        bool action_removal = true;     ///< Prune unreachable actions using RPG results
        bool use_interval_checker = true; ///< Interval-based formula evaluator (--smt-rpg-checker reverts to Z3)
        /// Stop when goals become achievable (before fixpoint). Default false:
        /// early termination is unsound for action removal because later RPG layers
        /// may discover actions reachable only through over-approximated bounds.
        bool early_goal_termination = false;
    } rpg;
    
    struct Planner {
        std::string strategy = "seq";  // Strategy name (replaces parallelism_strategy, encoder, propagator, interference)
        std::string mode = "satisficing";  // Search mode: satisficing, optimal, anytime
        std::string output_plan = "plan.txt";  // Plan output file path (IPC format)
        int max_steps = 500;
        int start_timestep = 0;  // Starting timestep for search (can be set by RPG lower bound)
        std::string horizon_schedule = "linear";  // Schedule type: linear|arithmetic|geometric|doubling
    } planner;
    
    struct Symmetry {
        bool enable_symmetries = false;  // Enable symmetry detection
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