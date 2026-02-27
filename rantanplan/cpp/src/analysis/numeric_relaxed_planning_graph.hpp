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

/**
 * @file numeric_relaxed_planning_graph.hpp
 * @brief Numeric Relaxed Planning Graph with SMT-based bounds computation
 *
 * Implements a relaxed planning graph that handles:
 * - Boolean fluents with ADL delete-relaxation (3-valued logic)
 * - Numeric fluents with relaxation (bounds via optimization)
 *
 * - Layer-by-layer construction until fixpoint
 * - Precise action applicability (full SMT satisfiability check)
 * - Two optimization queries per numeric variable per layer (min/max bounds)
 * - Supports early termination on goal achievement
  */

  // TODO: integrate ARPG to know when to stop. That is, the ARPG will tell us 
  // which bounds are unbounded, and therefore if all things that change between
  // two layers are unbounded, we can stop.

  // TODO: Improve how action applicability is computed. Currently, we either do
  // it one by one (many SMT calls), but we can do them in batch (one big SMT call)

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
     * @brief Bounds for numeric fluents (always bounded)
     *
     * Since initial state is fully defined, numeric variables start with
     * point bounds [value, value] and monotonically expand through effects.
     * Bounds are computed via Z3 optimization (minimize/maximize).
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
     * @param ctx Shared Z3 context (passed by reference)
     */
    NumericRelaxedPlanningGraph(Problem& problem, z3::context& ctx);
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

    /**
     * @brief Reset the graph to initial state (allows rebuilding)
     */
    void reset();

    // ========================================================================
    // QUERY METHODS - Goals
    // ========================================================================

    /**
     * @brief Check if all goal conditions are achievable
     *
     * Uses the same SMT-based approach as action applicability checking:
     * - Adds layer constraints
     * - Adds all goal expressions from problem_.goals()
     * - Checks satisfiability via Z3
     */
    bool are_goals_achievable() const;

    /**
     * @brief Get minimum number of steps (layer transitions) to achieve goals
     *
     * @return minimum steps, or -1 if goals not achievable
     */
    int get_minimum_steps_lower_bound() const;

    // ========================================================================
    // QUERY METHODS - Fluents
    // ========================================================================

    /**
     * @brief Get Boolean reachability at specific layer
     */
    BooleanReachability get_boolean_reachability(ExprID fluent_eid, int layer) const;

    /**
     * @brief Get reachability status at final layer
     */
    BooleanReachability get_boolean_reachability(ExprID fluent_eid) const;

    /**
     * @brief Get numeric bounds at specific layer
     */
    NumericBounds get_numeric_bounds(ExprID fluent_eid, int layer) const;

    /**
     * @brief Get numeric bounds at final layer
     */
    NumericBounds get_numeric_bounds(ExprID fluent_eid) const;

    // ========================================================================
    // QUERY METHODS - Actions
    // ========================================================================

    /**
     * @brief Get all actions that are applicable in a given layer
     */
    const std::vector<const Action*>& get_actions_in_layer(int layer) const;

    /**
     * @brief Check if action is applicable at specific layer
     */
    bool is_action_applicable(const Action& action, int layer) const;

    /**
     * @brief Get actions that can be safely removed (never applicable in any layer)
     */
    std::vector<const Action*> get_removable_actions() const;

    /**
     * @brief Remove unreachable actions from the problem
     * @return Number of actions removed
     */
    size_t remove_unreachable_actions();

    /**
     * @brief Get total number of layers
     */
    size_t get_layer_count() const { return layer_states_.size(); }

    // ========================================================================
    // DEBUG AND ANALYSIS
    // ========================================================================

    void print_debug_info() const;
    void print_boolean_evolution() const;
    void print_numeric_bounds_evolution() const;
    void print_reachability_analysis() const;
    void print_statistics() const;
    void print_layer_summary(int layer) const;
    void print_action_applicability(int layer, const std::vector<const Action*>& applicable) const;
    void print_layer_delta(int prev_layer, int curr_layer) const;

private:
    // ========================================================================
    // MEMBER VARIABLES - Problem Reference
    // ========================================================================

    Problem& problem_;

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

    z3::context& ctx_;  // SHARED: reference to external Z3 context
    std::unique_ptr<z3::optimize> z3_optimizer_;  // OWNED: optimizer for bounds
    Z3VariableFactory variable_factory_;  // OWNED: manages Z3 variables per layer
    mutable GroundedEncodingVisitor grounded_visitor_;  // OWNED: converts Expression → z3::expr (mutable for const methods)

    // Maps each fluent to the actions/effects that can modify it
    // Enables O(effects_for_fluent) lookup instead of O(all_actions * all_effects)
    std::unordered_map<ExprID, std::vector<std::pair<const Action*, const EffectExpression*>>> epc_index_;

    // ========================================================================
    // MEMBER VARIABLES - Configuration
    // ========================================================================

    int max_layers_;
    bool batch_action_applicability_;
    bool enable_all_actions_reachable_termination_;  // Enable early termination when all actions reachable + goals achieved

    // ========================================================================
    // MEMBER VARIABLES - Statistics
    // ========================================================================

    mutable double build_time_ms_;
    mutable size_t total_smt_queries_;
    mutable size_t total_optimization_queries_;
    mutable size_t total_applicability_checks_;

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
     * @brief Build next layer from current layer
     * @return true if new layer created, false if fixpoint reached
     */
    bool build_next_layer();

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
     * For each numeric fluent:
     * - Build SMT model with:
     *   - Previous layer state constraints
     *   - Effect constraints (disjunction of all effects + persistence)
     * - Query 1: minimize fluent value → lower bound
     * - Query 2: maximize fluent value → upper bound
     */
    void compute_numeric_bounds(
        const std::vector<const Action*>& applicable_actions,
        int prev_layer,
        int next_layer);

    /**
     * @brief Compute bounds for single numeric variable (two queries)
     */
    NumericBounds compute_single_variable_bounds(
        int fluent_id,
        const std::vector<const Action*>& applicable_actions,
        int prev_layer,
        int next_layer) const;

    /**
     * @brief Compute bounds using optimization (minimize or maximize)
     */
    double compute_bound_optimization(
        int fluent_id,
        const std::vector<const EffectExpression*>& effects,
        int prev_layer,
        int next_layer,
        bool minimize) const;

    /**
     * @brief Build SMT model for bounds computation
     *
     * Adds constraints:
     * - Previous layer state (Boolean reachability + numeric bounds)
     * - Effect constraints: fluent' = persistence OR effect1 OR effect2 OR ...
     * - Conditional effects handled as implications
     */
    void build_bounds_smt_model(
        int fluent_id,
        const std::vector<const Action*>& applicable_actions,
        int prev_layer,
        int next_layer) const;

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
     * @brief Convert ExprID to Z3 using visitor
     */
    z3::expr convert_expr_id_to_z3(ExprID eid, int layer) const;


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
