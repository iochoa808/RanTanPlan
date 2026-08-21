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
        bool persist_clauses = false;  // Theory lemmas discarded on backtrack; propagators re-derive eagerly
        double epsilon = 1e-6;        // Numerical tolerance for floating-point comparisons
        bool reachability_grounding = true;  // --up-grounding disables this
        bool numeric_grounding = true;       // --no-numeric-grounding disables this
        bool dump_problem = false;    // --dump-problem: print Problem::to_string() after pipeline and exit
        // [XTS] --array-frame-mode: which frame-axiom shape array/set fluents use.
        // Applies under both array_encoding backends below (Theory and UF each
        // implement both shapes, pointwise in the UF case since UF has no whole-
        // function equality to compare with !=.
        //   "disequality" (default) — (f^t != f^{t+1}) -> disjunction(actions), same shape as scalars.
        //   "ite"                   — total-update ITE chain (ported from branch z3-proves).
        std::string array_frame_mode = "disequality";
        // [XTS-UnFun] --array-encoding: which Z3 theory backs array/set fluents.
        //   "uf" (default)  — uninterpreted functions, one per (fluent, timestep), with
        //                     reads/writes/frame axioms built by pointwise domain
        //                     enumeration instead of select/store/extensionality.
        //   "theory"        — Z3 Array sort (select/store), see docs/z3_encoding_rationale.md.
        // Only the plain Grounded encoder implements the UF write paths, so a
        // chained/R2E strategy falls back to "theory" unless "uf" was asked for
        // explicitly (see array_encoding_explicit below and Config::validate).
        std::string array_encoding = "uf";
        // [XTS-UnFun] True once --array-encoding is passed on the command line.
        // Distinguishes "the user demanded uf" (hard error when the strategy cannot
        // provide it) from "uf is merely the default" (silent, logged fallback).
        bool array_encoding_explicit = false;
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
        /// Semantic goal-relevance analysis: uses AchieversAnalysis to remove
        /// actions that cannot contribute to any goal condition (even transitively).
        /// Runs after RPG action removal. Default true.
        bool goal_relevance = true;
        /// Maximum iterations for goal-relevance fixpoint loop.
        int goal_relevance_max_iterations = 10;
        /// Directional widening threshold: after this many consecutive expansions
        /// of the same side (lower/upper) of a numeric fluent's bounds, snap to +-inf.
        int widening_threshold = 100;
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

    struct LocalReasoning {
        bool enabled = true;              ///< Master switch (--no-local-reasoning disables)
        bool rpg_bounds = true;           ///< Inject RPG fixpoint bounds at each timestep
        bool mutex_discovery = true;      ///< Tier 1 syntactic mutex detection
    } local_reasoning;

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