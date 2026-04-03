#pragma once

#include "../problem/problem.hpp"
#include "../problem/action.hpp"
#include "../problem/effect_expression.hpp"
#include "../encoders/z3_variable_factory.hpp"
#include "../encoders/grounded_encoding_visitor.hpp"
#include "../config/config.hpp"
#include <z3++.h>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <cmath>
#include "../grounding/interval.hpp"

/**
 * @file numeric_relaxed_planning_graph.hpp
 * @brief Numeric Relaxed Planning Graph with hybrid interval/SMT architecture
 *
 * Layer-by-layer delete-relaxed planning graph:
 *
 * - Boolean fluents: 3-valued reachability (FALSE_ONLY / TRUE_ONLY / BOTH)
 *   with monotonic transitions under ADL delete-relaxation.
 *
 * - Numeric fluents: interval arithmetic for bound propagation. Each effect
 *   branch is evaluated as an interval; layer result is convex union of all
 *   branches including persistence. No Z3 calls for bound propagation.
 *
 * - Action applicability / goal checks: by default, interval-based 3-valued
 *   formula evaluation. --smt-rpg-checker reverts to per-action Z3 SAT queries
 *   for cases where joint multi-variable constraints need exact reasoning.
 *
 * - Directional widening: per-fluent, per-side expansion counters. After
 *   WIDENING_THRESHOLD (3) consecutive expansions, the side snaps to +-inf.
 *   Frozen sides (from lifted effect analysis) are exempt. Guarantees
 *   termination for both linear and non-linear effects.
 *
 * Termination criteria (independent, both on by default):
 *   1. Config::RPG::early_goal_termination: stop when goals are achievable.
 *   2. set_stop_when_all_reachable(true): stop when all actions are reachable
 *      AND goals are achievable.
 * Disabling both runs to true fixpoint (needed for sound variable bounds
 * in AchieversAnalysis and cost lower bounds).
 */

namespace rantanplan {

class NumericRelaxedPlanningGraph {
public:
    /**
     * @brief Three-valued logic for Boolean fluent reachability
     *
     * Under ADL delete-relaxation, Boolean fluents can be in one of three states:
     * - FALSE_ONLY: Can only be false (not yet made true by any action)
     * - TRUE_ONLY: Can only be true (made true, no negative effect yet)
     * - BOTH: Can be either true or false (both possibilities reachable)
     *
     * State transitions (monotonic):
     * - FALSE_ONLY + positive effect → BOTH
     * - FALSE_ONLY + negative effect → FALSE_ONLY (no change)
     * - TRUE_ONLY + positive effect → TRUE_ONLY (no change)
     * - TRUE_ONLY + negative effect → BOTH
     * - BOTH + any effect → BOTH (absorbing state)
     */
    enum class BooleanReachability {
        FALSE_ONLY,    // Can only be false
        TRUE_ONLY,     // Can only be true
        BOTH           // Can be either true or false
    };

    /**
     * @brief 3-valued result for interval-based formula evaluation.
     *
     * Used by the interval checker (non-Z3) to determine if a formula
     * is satisfiable given box constraints (independent per-fluent bounds).
     */
    enum class FormulaResult {
        ALWAYS_TRUE,   // True for ALL assignments within box constraints
        ALWAYS_FALSE,  // False for ALL assignments (= definitely UNSAT)
        UNKNOWN        // Truth depends on specific assignment (= possibly SAT)
    };

    /**
     * @brief Bounds for numeric fluents.
     *
     * Initial state has point bounds [value, value]. Effects monotonically
     * expand bounds via interval arithmetic. Directional widening snaps
     * persistently-expanding sides to +-infinity for termination.
     */
    struct NumericBounds {
        double lower;
        double upper;

        NumericBounds() : lower(0.0), upper(0.0) {}
        NumericBounds(double value) : lower(value), upper(value) {}
        NumericBounds(double l, double u) : lower(l), upper(u) {}

        bool is_point() const { 
            double epsilon = Config::instance().global.epsilon;
            return std::abs(upper - lower) < epsilon;
        }
        double width() const { return upper - lower; }

        bool contains(double value) const {
            double epsilon = Config::instance().global.epsilon;
            return value >= lower - epsilon && value <= upper + epsilon;
        }

        bool operator==(const NumericBounds& other) const {
            // Fast path: exact equality handles +-infinity correctly
            // (std::abs(inf - inf) is NaN, which would fail epsilon check)
            if (lower == other.lower && upper == other.upper) return true;
            double epsilon = Config::instance().global.epsilon;
            return std::abs(lower - other.lower) < epsilon &&
                   std::abs(upper - other.upper) < epsilon;
        }

        bool operator!=(const NumericBounds& other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief Complete state representation for a single layer
     */
    struct LayerState {
        // Boolean fluent reachability
        std::unordered_map<int, BooleanReachability> boolean_reachability;

        // Numeric fluent bounds (always bounded)
        std::unordered_map<int, NumericBounds> numeric_bounds;

        // Helper: check if two layer states are equal (for fixpoint detection)
        bool operator==(const LayerState& other) const;
    };

    // ========================================================================
    // CONSTRUCTION
    // ========================================================================

    /**
     * @brief Construct a numeric relaxed planning graph
     * @param problem The planning problem (grounded)
     */
    explicit NumericRelaxedPlanningGraph(const Problem& problem);
    ~NumericRelaxedPlanningGraph() = default;

    // Disable copying (Z3 context is not copyable)
    NumericRelaxedPlanningGraph(const NumericRelaxedPlanningGraph&) = delete;
    NumericRelaxedPlanningGraph& operator=(const NumericRelaxedPlanningGraph&) = delete;

    // ========================================================================
    // MAIN INTERFACE
    // ========================================================================

    /**
     * @brief Build the relaxed planning graph from initial state until fixpoint
     *
     * Layer-by-layer construction:
     * 1. Initialize layer 0 from initial state
     * 2. For each layer:
     *    - Compute applicable actions
     *    - Propagate Boolean effects and compute numeric bounds
     *    - Check fixpoint or goals (if early termination enabled)
     *
     * @return true if all goals are reachable, false otherwise
     */
    bool build();

    // ========================================================================
    // QUERY METHODS
    // ========================================================================

    bool are_goals_achievable() const;
    int get_minimum_steps_lower_bound() const;
    const std::vector<const Action*>& get_actions_in_layer(int layer) const;
    std::vector<size_t> get_removable_action_indices() const;

    /**
     * @brief Get total number of layers
     */
    size_t get_layer_count() const { return layer_states_.size(); }

    /**
     * @brief Get first layer at which each action becomes applicable.
     * @return Map from action ID to first applicable layer index.
     */
    std::unordered_map<int, int> get_action_first_layers() const;

    /// Actions ordered by first layer of applicability (stable sort by action ID within layer).
    std::vector<const Action*> get_action_ordering() const;

    /**
     * @brief Get state variable bounds at the final layer, keyed by ExprID.
     *
     * Returns both numeric interval bounds and boolean reachability
     * (booleans with TRUE_ONLY or BOTH get interval [1,1]; FALSE_ONLY
     * are omitted since they contribute no constraint).
     *
     * Format matches what AchieversAnalysis expects for SMT bound constraints.
     */
    std::unordered_map<ExprID, Interval> get_state_variable_bounds() const;

    // ========================================================================
    // INTERVAL EVALUATION (public for cost bound analysis)
    // ========================================================================

    /**
     * @brief Evaluate an ExprID to an Interval using current layer bounds
     *
     * Walks the expression tree:
     *   - Constants -> point interval [c, c]
     *   - State variables -> lookup in layer's numeric_bounds
     *   - Arithmetic (+, -, *, /) -> interval arithmetic
     *   - Unknown/unsupported -> Interval::unbounded()
     *
     * For sound lower bounds on cost expressions, call at the final layer
     * after building with early termination disabled.
     */
    Interval evaluate_interval(ExprID eid, int layer) const;

    /// Stop when all actions are reachable AND goals are achievable (default: true).
    /// Disabling this causes build() to run to true fixpoint.
    void set_stop_when_all_reachable(bool enable) { stop_when_all_reachable_ = enable; }

    // ========================================================================
    // DEBUG AND ANALYSIS
    // ========================================================================

    void print_statistics() const;
    void print_layer_summary(int layer) const;
    void print_action_applicability(int layer, const std::vector<const Action*>& applicable) const;
    void print_layer_delta(int prev_layer, int curr_layer) const;

private:
    // ========================================================================
    // MEMBER VARIABLES - Problem Reference
    // ========================================================================

    const Problem& problem_;

    // ========================================================================
    // MEMBER VARIABLES - State Tracking
    // ========================================================================

    // Layer states (Boolean reachability + numeric bounds)
    std::vector<LayerState> layer_states_;

    // Action tracking: which actions are applicable at each layer
    std::vector<std::vector<const Action*>> action_layers_;

    // Fluent classification
    std::unordered_set<int> boolean_fluent_ids_;
    std::unordered_set<int> numeric_fluent_ids_;

    // ========================================================================
    // MEMBER VARIABLES - Z3 Infrastructure
    // ========================================================================

    mutable z3::context ctx_;              // Owned Z3 context (mutable: Z3 API is not const-correct)
    Z3VariableFactory variable_factory_;   // Z3 variable management per layer
    mutable GroundedEncodingVisitor grounded_visitor_;  // ExprID -> z3::expr (mutable for const methods)

    // Maps each fluent to the actions/effects that can modify it
    // Enables O(effects_for_fluent) lookup instead of O(all_actions * all_effects)
    std::unordered_map<ExprID, std::vector<std::pair<const Action*, const EffectExpression*>>> epc_index_;

    // ========================================================================
    // MEMBER VARIABLES - Directional Widening & Freeze Analysis
    // ========================================================================

    /// Per-fluent (by fluent_id) expansion counters for directional widening.
    std::unordered_map<int, int> lower_expansion_count_;
    std::unordered_map<int, int> upper_expansion_count_;

    /// Fluent schema IDs whose upper/lower bound should never be widened.
    /// Populated by precompute_freezes() at construction time.
    std::unordered_set<int> freeze_upper_;
    std::unordered_set<int> freeze_lower_;

    /// Maps ground fluent_id -> fluent schema ID (for freeze lookup).
    std::unordered_map<int, int> fluent_schema_map_;

    static constexpr int WIDENING_THRESHOLD = 8;

    // ========================================================================
    // MEMBER VARIABLES - Configuration
    // ========================================================================

    int max_layers_;
    bool stop_when_all_reachable_;  ///< Stop when all actions reachable + goals achieved

    // ========================================================================
    // MEMBER VARIABLES - Statistics
    // ========================================================================

    mutable double build_time_ms_;
    mutable size_t total_smt_queries_;
    mutable size_t total_applicability_checks_;
    mutable size_t total_interval_checks_;

    // ========================================================================
    // STATIC MEMBERS
    // ========================================================================

    static const std::vector<const Action*> empty_action_vector_;

    // ========================================================================
    // PRIVATE METHODS - Initialization
    // ========================================================================

    /**
     * @brief Build EPC (Effect-Precondition-Constraint) index
     *
     * Maps each fluent to the actions/effects that can modify it.
     * Borrowed pattern from GroundedEncoder for efficient effect lookup.
     */
    void build_epc_index();

    /**
     * @brief Classify all fluents into Boolean vs numeric
     */
    void classify_fluents();

    /**
     * @brief Initialize layer 0 from fully-defined initial state
     */
    void initialize_layer_0();

    // ========================================================================
    // PRIVATE METHODS - Main Algorithm
    // ========================================================================

    /**
     * @brief Check if fixpoint has been reached
     *
     * Compares last two layers:
     * - Boolean: all reachability values unchanged
     * - Numeric: all bounds unchanged
     */
    bool is_fixpoint_reached() const;

    /**
     * @brief Check if all actions are now reachable
     * @return true if total actions reached equals total actions in problem
     */
    bool are_all_actions_reachable() const;

    // ========================================================================
    // PRIVATE METHODS - Action Applicability
    // ========================================================================

    /**
     * @brief Compute which actions are applicable at current layer (SMT-based)
     *
     * For each action:
     * - Build SMT query with layer state constraints
     * - Add action precondition
     * - Check satisfiability
     * - If SAT, action is applicable
     */
    std::vector<const Action*> compute_applicable_actions(int layer) const;

    /**
     * @brief Compute applicable actions individually (one query per action)
     */
    std::vector<const Action*> compute_applicable_actions_individual(int layer) const;

    /**
     * @brief Compute applicable actions in batch (one query with action variables)
     */
    std::vector<const Action*> compute_applicable_actions_batch(int layer) const;

    /**
     * @brief Check if single action is applicable at layer (SMT query)
     */
    bool is_action_applicable_smt(const Action& action, int layer) const;

    // ========================================================================
    // PRIVATE METHODS - Interval-Based Formula Evaluation
    // ========================================================================

    /**
     * @brief Evaluate a formula to a 3-valued result using interval arithmetic.
     *
     * Recursively walks the ExprID tree:
     *   - Boolean SVs: lookup BooleanReachability in layer state
     *   - Numeric comparisons: evaluate sides to intervals, check tautology/falsehood
     *   - AND/OR/NOT/IMPLIES: compose with 3-valued logic
     *
     * Sound over-approximation: never returns ALWAYS_FALSE when the formula
     * is actually satisfiable. May return UNKNOWN when Z3 would prove UNSAT
     * (only for joint multi-variable numeric constraints).
     */
    FormulaResult evaluate_formula_interval(ExprID eid, int layer) const;

    /**
     * @brief Check if a numeric comparison is always true, always false, or unknown.
     */
    FormulaResult evaluate_comparison_interval(ExprOperator op,
                                               const Interval& lhs,
                                               const Interval& rhs) const;

    /**
     * @brief Interval-based action applicability check.
     */
    bool is_action_applicable_interval(const Action& action, int layer) const;

    /**
     * @brief Interval-based goal achievability check at a specific layer.
     */
    bool are_goals_achievable_at_layer_interval(int layer) const;

    // ========================================================================
    // PRIVATE METHODS - Boolean Effect Propagation
    // ========================================================================

    /**
     * @brief Propagate Boolean effects from applicable actions
     *
     * For each Boolean fluent affected by applicable actions:
     * - Check if any action has positive effect → transition to BOTH if needed
     * - Check if any action has negative effect → transition to BOTH if needed
     * - Apply delete-relaxation semantics (monotonic transitions)
     */
    void propagate_boolean_effects(
        const std::vector<const Action*>& applicable_actions,
        int prev_layer,
        int next_layer);

    /**
     * @brief Apply single Boolean effect with delete-relaxation semantics
     */
    void apply_boolean_effect(
        const EffectExpression& effect,
        BooleanReachability current_state,
        BooleanReachability& next_state,
        int layer) const;

    // ========================================================================
    // PRIVATE METHODS - Numeric Bounds Computation
    // ========================================================================

    /**
     * @brief Compute numeric bounds for all numeric fluents at next layer
     *
     * Uses interval arithmetic instead of Z3 optimize:
     * For each numeric fluent, evaluates all applicable effects as intervals,
     * takes convex union with persistence, then applies directional widening.
     */
    void compute_numeric_bounds(
        const std::vector<const Action*>& applicable_actions,
        int prev_layer,
        int next_layer);

    /**
     * @brief Compute bounds for single numeric variable via interval arithmetic
     *
     * Evaluates persistence ∪ effect1 ∪ effect2 ∪ ... as intervals.
     */
    NumericBounds compute_single_variable_bounds_interval(
        int fluent_id,
        const std::vector<const Action*>& applicable_actions,
        int prev_layer) const;

    /**
     * @brief Apply directional widening to new bounds for a fluent
     *
     * Tracks per-side expansion counts. After WIDENING_THRESHOLD consecutive
     * expansions of a side, snaps it to +-infinity. Frozen sides are exempt.
     */
    void apply_widening(int fluent_id, NumericBounds& new_bounds, const NumericBounds& prev_bounds);

    /**
     * @brief Lifted freeze pre-analysis (adapted from NumericBoundsIndex)
     *
     * Determines which fluent schemas can never have their upper/lower
     * bound moved by any effect. Populates freeze_upper_/freeze_lower_.
     */
    void precompute_freezes();

    /**
     * @brief Build fluent_schema_map_ for ground fluent ID -> schema ID mapping
     */
    void build_fluent_schema_map();

    // ========================================================================
    // PRIVATE METHODS - Helper Methods for Effects
    // ========================================================================

    /**
     * @brief Get all effects that can modify a given fluent
     *
     * Uses EPC index for efficient lookup: O(effects_for_fluent)
     * Filters by applicable actions only.
     */
    std::vector<const EffectExpression*> get_effects_for_fluent(
        int fluent_id,
        const std::vector<const Action*>& actions) const;

    // ========================================================================
    // PRIVATE METHODS - Expression Analysis
    // ========================================================================

    /**
     * @brief Check if expression is a Boolean condition
     */
    bool is_boolean_expression(ExprID eid) const;

    /**
     * @brief Check if expression is a numeric expression
     */
    bool is_numeric_expression(ExprID eid) const;

    /**
     * @brief Get fluent ID from ExprID (for indexing)
     */
    int find_grounded_fluent_id(ExprID fluent_eid) const;

    // ========================================================================
    // PRIVATE METHODS - SMT Constraint Building
    // ========================================================================

    /**
     * @brief Add layer state constraints to Z3 optimizer
     *
     * Adds constraints for:
     * - Boolean reachability (for each Boolean fluent based on its state)
     * - Numeric bounds (for each numeric fluent)
     */
    void add_layer_constraints(z3::solver& solver, int layer) const;

    /**
     * @brief Add Boolean reachability constraints
     */
    void add_boolean_constraints(z3::solver& solver, int layer) const;

    /**
     * @brief Add numeric bounds constraints
     */
    void add_numeric_constraints(z3::solver& solver, int layer) const;

    /**
     * @brief Extract numeric value from Z3 model
     *
     * Converts Z3 expression (int, real, fraction) to double.
     * Note: Exact conversion strategy for fractions and algebraic numbers is TBD.
     * Currently assumes Z3 returns decimal approximation.
     */
    double extract_numeric_value(const z3::expr& z3_value) const;

    // ========================================================================
    // PRIVATE METHODS - Utilities
    // ========================================================================

    /**
     * @brief Convert BooleanReachability to string (for debugging)
     */
    std::string reachability_to_string(BooleanReachability r) const;
};

} // namespace rantanplan
