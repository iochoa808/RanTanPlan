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
 * @brief Numeric Relaxed Planning Graph with hybrid interval/SMT bounds
 *
 * Implements a relaxed planning graph that handles:
 * - Boolean fluents with ADL delete-relaxation (3-valued logic)
 * - Numeric fluents with interval arithmetic for bound propagation
 * - SMT-based joint satisfiability for action applicability and goals
 * - Directional widening for guaranteed termination
 *
 * ==========================================================================
 * IMPLEMENTATION PLAN: Hybrid Interval/SMT with Directional Widening
 * ==========================================================================
 *
 * Overview
 * --------
 * Replace the current "2 Z3 optimize calls per fluent per layer" bound
 * propagation with fast interval arithmetic, while keeping Z3 only for
 * action applicability and goal reachability checks where joint
 * satisfiability across multiple fluents genuinely matters. Add
 * directional widening (adapted from the grounding layer) to guarantee
 * termination for both linear and non-linear effects.
 *
 * Current cost model
 * ------------------
 * For a problem with F numeric fluents, A actions, and L layers:
 *   - Bound propagation: 2*F Z3 optimize calls per layer = 2*F*L total
 *   - Action applicability: A Z3 sat calls per layer = A*L total
 *   - Goal checks: ~L Z3 sat calls
 *   Total: (2F + A + 1) * L Z3 calls
 *
 * For a problem with 20 fluents, 50 actions, 50 layers: 4,550 Z3 calls.
 * The bound propagation accounts for 2,000 of those (44%).
 *
 * Target cost model (hybrid)
 * --------------------------
 *   - Bound propagation: F interval-arithmetic operations per layer
 *     (microseconds, no Z3)
 *   - Action applicability: A Z3 sat calls per layer (unchanged)
 *   - Goal checks: ~L Z3 sat calls (unchanged)
 *   Total: (A + 1) * L Z3 calls + negligible interval arithmetic
 *
 * Same example: 2,550 Z3 calls (44% reduction). The savings are
 * proportionally larger when F is large relative to A, which is common
 * in numeric-heavy domains.
 *
 * Why hybrid: motivating example
 * ------------------------------
 * Consider a logistics domain with numeric fuel tracking:
 *
 *   Fluents: fuel(truck1), fuel(truck2), load(truck1), load(truck2),
 *            distance(A,B), capacity(truck1), capacity(truck2)
 *   Actions:
 *     drive(truck1, A, B):
 *       pre: at(truck1, A), fuel(truck1) >= distance(A,B)
 *       eff: decrease(fuel(truck1), distance(A,B)),
 *            not at(truck1, A), at(truck1, B)
 *     load_pkg(truck1, pkg1, A):
 *       pre: at(truck1, A), at(pkg1, A), load(truck1) < capacity(truck1)
 *       eff: increase(load(truck1), 1), not at(pkg1, A), in(pkg1, truck1)
 *     refuel(truck1, A):
 *       pre: at(truck1, A), has_station(A)
 *       eff: assign(fuel(truck1), 100)
 *
 * Bound propagation: For fuel(truck1) at each layer, the question is
 * "what is the min/max fuel can be?" Given fuel ∈ [20, 100] and
 * distance(A,B) = 30:
 *   - drive decreases: fuel' = fuel - 30 → [20-30, 100-30] = [-10, 70]
 *   - refuel assigns: fuel' = 100
 *   - persistence: fuel' ∈ [20, 100]
 *   - Convex union: [-10, 100]
 *
 * Interval arithmetic gives [-10, 100]. Z3 optimize would give the same
 * result here — the min/max are determined by the individual branch
 * extremes, which interval arithmetic captures exactly. Z3 adds nothing.
 *
 * Where Z3 adds nothing: any time the bound of each branch can be
 * computed independently. This is the common case for bound propagation
 * because each effect typically modifies one fluent at a time.
 *
 * Action applicability: "Is drive(truck1, A, B) applicable?" requires
 * checking fuel(truck1) >= distance(A,B) AND at(truck1, A). The boolean
 * part is trivial, but the numeric part asks: given fuel ∈ [-10, 100]
 * and distance ∈ [30, 30], is there a point where fuel >= distance?
 * Answer: yes (any fuel in [30, 100] works).
 *
 * Where Z3 genuinely helps: the load_pkg action has precondition
 * load(truck1) < capacity(truck1). If load ∈ [0, 5] and capacity = 4
 * (constant), interval arithmetic would check "is there x ∈ [0, 5]
 * and y = 4 such that x < y?" and answer yes (x=0 works). But suppose
 * a more complex precondition involves MULTIPLE numeric fluents
 * together, like:
 *
 *   pre: fuel(truck1) >= distance(A,B) + load(truck1) * fuel_per_kg
 *
 * Here fuel, load, and distance interact. Checking satisfiability of
 * this conjunction over their independent intervals is what Z3 does
 * well — it finds a consistent assignment across all variables
 * simultaneously. Interval arithmetic checking each comparison in
 * isolation would miss cases where the intervals overlap individually
 * but no single point satisfies all constraints jointly.
 *
 * Summary: interval arithmetic for propagation (per-fluent, independent
 * branches, exact or near-exact), Z3 for satisfiability (multi-fluent
 * joint constraints, where correlations matter).
 *
 * Directional widening
 * --------------------
 * Problem: numeric fluents with unbounded effects (constant additive or
 * non-linear) cause intervals to grow every layer without converging.
 * E.g., increment(counter, 1) causes upper bound to grow by 1 per
 * layer, requiring 100 layers for goal value >= 100.
 *
 * Solution: adapted from NumericBoundsIndex (grounding/numeric_bounds_
 * index.{hpp,cpp}). Per fluent, per side (lower/upper), track how many
 * consecutive layers the bound has moved. After a threshold, snap that
 * side to +-infinity.
 *
 * For each numeric fluent f, maintain:
 *   lower_expansion_count[f] -- incremented when bounds[f].lower shrinks
 *   upper_expansion_count[f] -- incremented when bounds[f].upper grows
 *
 * After computing bounds at layer k+1 via interval arithmetic:
 *   1. If bounds[k+1].lower < bounds[k].lower: lower_count[f]++
 *   2. If bounds[k+1].upper > bounds[k].upper: upper_count[f]++
 *   3. If lower_count[f] >= WIDENING_THRESHOLD and not frozen:
 *      set bounds[k+1].lower = -infinity
 *   4. If upper_count[f] >= WIDENING_THRESHOLD and not frozen:
 *      set bounds[k+1].upper = +infinity
 *   5. Once a side is at infinity, it never changes (contributes to
 *      fixpoint).
 *
 * WIDENING_THRESHOLD is an internal constant (not CLI-exposed), default
 * 3, matching the grounding layer.
 *
 * Ceiling/floor freezing
 * ----------------------
 * Lifted effect analysis (same logic as NumericBoundsIndex::
 * precompute_freezes()) determines which fluent schemas can never have
 * their upper/lower bound moved by any effect:
 *   - INCREASE with value range [a,b]: if b > 0, can't freeze upper;
 *     if a < 0, can't freeze lower.
 *   - DECREASE with value range [a,b]: if b > 0, can't freeze lower;
 *     if a < 0, can't freeze upper.
 *   - ASSIGN with unbounded value: can't freeze either side.
 * Frozen sides are never widened, preserving finite bounds where
 * structurally guaranteed.
 *
 * Termination guarantee
 * ---------------------
 * After at most (WIDENING_THRESHOLD * |numeric_fluents|) layers of
 * non-convergence, every non-frozen side reaches infinity. At that
 * point the only changing things are boolean reachability (finite set)
 * and frozen numeric sides (bounded by distinct constant-effect values).
 * Both are finite, so fixpoint is guaranteed.
 *
 * Unsolvability can ONLY be proven at fixpoint: if all layers have been
 * computed, the state is stable, and goals are still unreachable, then
 * the problem is proven unsolvable. Stopping early (by goal achievement
 * or max_layers) cannot prove unsolvability.
 *
 * Stopping criterion
 * ------------------
 * Primary: goal reachability (first layer where are_goals_achievable()
 * returns true). Widening accelerates this by quickly expanding
 * intervals to cover goal thresholds.
 *
 * Secondary: fixpoint. With widening, fixpoint is guaranteed reachable.
 * If fixpoint is reached and goals are still unreachable, the problem
 * is proven unsolvable.
 *
 * At the goal layer, the graph provides:
 *   - Lower bound on plan length (layer number)
 *   - RPG graph structure for landmark extraction
 *   - Numeric bounds at goal layer
 *   - Action reachability per layer
 *
 * Effect on precision
 * -------------------
 * Two sources of precision change vs. current implementation:
 *
 * 1. Interval arithmetic vs Z3 optimize for bounds: interval arithmetic
 *    over-approximates when effects have cross-fluent dependencies. In
 *    practice this is rare for bound propagation (effects typically
 *    modify one fluent). The over-approximation is sound.
 *
 * 2. Widening: after threshold layers, widened sides go to +-infinity.
 *    Goals may appear reachable earlier (weaker lower bound). Sound.
 *
 * Worked example: Counters domain
 * --------------------------------
 * Domain: counter c, initial value 0, goal value(c) >= 100.
 * Actions: increment(c) with effect increase(value(c), 1).
 *          decrement(c) with effect decrease(value(c), 1),
 *                       precondition value(c) >= 1.
 *
 * Freeze analysis:
 *   - increment: INCREASE by 1 ([1,1]). upper > 0 -> can't freeze
 *     upper. lower >= 0 -> CAN freeze lower.
 *   - decrement: DECREASE by 1 ([1,1]). upper > 0 -> can't freeze
 *     lower. lower >= 0 -> CAN freeze upper.
 *   - Combined: can_freeze_upper = false (increment raises it),
 *     can_freeze_lower = false (decrement lowers it).
 *   - Neither side frozen.
 *
 * Wait -- that seems wrong for this domain since decrement has
 * precondition value(c) >= 1 which prevents going below 0. But freeze
 * analysis is purely syntactic (effect structure only, ignores
 * preconditions). This is sound: we may widen a side that wouldn't
 * actually move, but never fail to widen a side that would. The
 * conservatism costs precision, not correctness.
 *
 * Without widening (current behavior):
 *   Layer 0:  value(c) = [0, 0]
 *   Layer 1:  value(c) = [0, 1]     (increment available, decrement not)
 *   Layer 2:  value(c) = [-1, 2]    (decrement now available)
 *   ...
 *   Layer 100: value(c) = [-99, 100] <- goals reachable
 *   200 Z3 optimize calls + 200 applicability calls = 400 Z3 calls
 *
 * With hybrid + widening (threshold = 3):
 *   Layer 0:  [0, 0]      (interval arithmetic: free)
 *   Layer 1:  [0, 1]      upper_count=1 (interval: free)
 *   Layer 2:  [-1, 2]     upper_count=2, lower_count=1
 *   Layer 3:  [-2, 3]     upper_count=3 -> WIDEN upper to +inf
 *                          lower_count=2
 *   Layer 4:  [-3, +inf)  lower_count=3 -> WIDEN lower to -inf
 *                          [-inf, +inf)
 *   Layer 5:  [-inf, +inf). Fixpoint on numeric side.
 *             Goal value(c) >= 100: trivially satisfiable.
 *             -> STOP, goals reachable.
 *   0 Z3 optimize calls + ~10 applicability calls = ~10 Z3 calls
 *
 * Lower bound: 5 layers (weaker than true minimum of 100). For tighter
 * bounds with constant effects, use action counting (LM.md Part 5).
 *
 * Worked example: Zenotravel with fuel
 * -------------------------------------
 * Fluent fuel(plane1), initial 100. max_fuel = 200 (constant).
 *   fly(p1,c1,c2): decrease(fuel, distance(c1,c2) * slow_burn)
 *                   -- state-dependent, non-linear
 *   refuel(p1,c1): assign(fuel, max_fuel)
 *                   -- constant value 200
 *
 * Freeze analysis:
 *   - fly: DECREASE with state-dependent (unbounded) value ->
 *     can't freeze either side.
 *   - refuel: ASSIGN with constant 200 -> bounded, OK.
 *   - Combined: neither side frozen (conservative).
 *
 * With hybrid + widening (threshold = 3):
 *   Layer 0: [100, 100]
 *   Layer 1: [100-d, 200]  (fly decreases by some amount, refuel
 *            assigns 200). Interval arithmetic:
 *            - persistence: [100, 100]
 *            - fly: [100, 100] - eval(distance*burn) = [100-d, 100]
 *              where d = interval eval of distance*burn from current
 *              state bounds
 *            - refuel: [200, 200] (assign constant)
 *            - convex union: [100-d, 200]
 *            upper_count=1 (200 > 100), lower_count=1
 *   Layer 2: [lower', 200]. Upper stays at 200 because the only way
 *            to raise fuel is refuel which caps at 200. upper_count
 *            stays at 1. lower_count=2.
 *   Layer 3: lower shrinks again, lower_count=3 -> WIDEN lower.
 *            fuel = [-inf, 200]. upper_count still 1.
 *   Layer 4: [-inf, 200]. Numeric fixpoint. Only boolean changes
 *            drive further layers.
 *
 * Upper bound preserved at 200 without freezing: the expansion counter
 * for upper never hits threshold because refuel caps fuel and no
 * effect raises it further. Directional widening only widens the
 * moving side.
 *
 * Why hybrid helps here: at each layer, fuel bounds are computed by
 * evaluating "persistence ∪ fly_effect ∪ refuel_effect" via interval
 * arithmetic. The fly effect involves distance * slow_burn, which is
 * a non-linear expression evaluated over intervals -- interval
 * multiplication handles this directly. No Z3 optimize call needed.
 * Z3 is only called for action applicability: "is fly applicable
 * given fuel ∈ [-inf, 200] and distance ∈ [d1, d2]?" -- a joint
 * satisfiability check that Z3 handles well.
 *
 * Integration steps
 * -----------------
 * All changes within this class. No new files needed.
 *
 * Step 1: Add interval evaluation infrastructure.
 *
 *   Add a method to evaluate an ExprID to an Interval using current
 *   layer bounds. Reuse the Interval class from grounding/interval.hpp.
 *   The evaluator walks the expression tree:
 *     - Constants -> point interval [c, c]
 *     - State variables -> lookup in current layer's numeric_bounds,
 *       converting NumericBounds to Interval
 *     - Arithmetic (+, -, *) -> interval arithmetic
 *     - Unknown/unsupported -> Interval::unbounded() (safe fallback)
 *
 *   This mirrors RelaxedState::evaluate_expression() from arpg.hpp
 *   but operates on NumericBounds and ground fluent IDs instead of
 *   string-keyed variables.
 *
 * Step 2: Replace compute_single_variable_bounds() with interval
 *         propagation.
 *
 *   For each numeric fluent f with effects from applicable actions:
 *     branches = { persistence: current_bounds[f] }
 *     for each (action, effect) in epc_index_[f]:
 *       if action is applicable at this layer:
 *         switch effect.kind():
 *           ASSIGN:   branches.add(evaluate_interval(effect.value))
 *           INCREASE: branches.add(current + evaluate_interval(value))
 *           DECREASE: branches.add(current - evaluate_interval(value))
 *     new_bounds = convex_union(all branches)
 *
 *   This replaces 2 Z3 optimize calls with O(effects) interval ops.
 *
 * Step 3: Add widening state and freeze analysis.
 *
 *   Member variables:
 *     std::unordered_map<int, int> lower_expansion_count_;
 *     std::unordered_map<int, int> upper_expansion_count_;
 *     std::unordered_set<int> freeze_upper_;
 *     std::unordered_set<int> freeze_lower_;
 *     static constexpr int WIDENING_THRESHOLD = 3;
 *
 *   At construction: run precompute_freezes() using the same logic
 *   as NumericBoundsIndex::precompute_freezes(). ~60 lines, can
 *   duplicate or extract to shared utility.
 *
 *   The freeze analysis is per schema (lifted). Map to ground IDs:
 *   for each ground fluent ID, find its schema via the ExprPool and
 *   check if that schema is frozen.
 *
 * Step 4: Apply widening after interval propagation.
 *
 *   After computing new_bounds for fluent f at layer k+1:
 *     prev = layer_states_[k].numeric_bounds[f];
 *     if (new_bounds.lower < prev.lower) lower_expansion_count_[f]++;
 *     if (new_bounds.upper > prev.upper) upper_expansion_count_[f]++;
 *     if (!freeze_lower_[f] && lower_count[f] >= WIDENING_THRESHOLD)
 *       new_bounds.lower = -infinity;
 *     if (!freeze_upper_[f] && upper_count[f] >= WIDENING_THRESHOLD)
 *       new_bounds.upper = +infinity;
 *
 * Step 5: Handle infinity in add_numeric_constraints().
 *
 *   When emitting Z3 constraints for a layer (used by applicability
 *   and goal checks), skip the constraint for an infinite bound:
 *     if (!std::isinf(bounds.lower))
 *       solver.add(fluent >= real_val(bounds.lower));
 *     if (!std::isinf(bounds.upper))
 *       solver.add(fluent <= real_val(bounds.upper));
 *
 * Step 6: Fix NumericBounds::operator== for infinity.
 *
 *   std::abs(inf - inf) is NaN. Add exact-equality fast path:
 *     if (lower == other.lower && upper == other.upper) return true;
 *     // then epsilon check for finite values
 *
 * Step 7: Remove Z3 optimizer dependency from bound propagation.
 *
 *   The z3_optimizer_ member and compute_bound_optimization() method
 *   become unused for bound propagation. Keep them if needed for other
 *   purposes, or remove to simplify. The Z3 context and solver are
 *   still needed for applicability/goal checks.
 *
 * Validation
 * ----------
 * 1. Correctness: interval arithmetic over-approximates (convex union
 *    of independent branch evaluations). Widening further over-
 *    approximates. Both are sound for the RPG's purpose.
 *    - Goals unreachable at fixpoint -> proven unsolvable (sound).
 *    - Goals reachable -> valid (may be earlier layer than without
 *      widening, giving weaker lower bound but never incorrect).
 *
 * 2. Testing: `python test.py` before and after. All tests must pass.
 *    The RPG analysis changes but plan correctness is unaffected
 *    (the RPG only informs preprocessing, not the solver itself).
 *
 * 3. Performance: compare RPG build times, layer counts, and total
 *    Z3 calls on numeric benchmarks (counters, zenotravel, sailing)
 *    before (pure Z3) and after (hybrid + widening). Expect large
 *    reductions in Z3 calls and build time.
 *
 * 4. Precision regression: lower bounds from get_minimum_steps_lower_
 *    bound() should be <= bounds without widening (weaker but sound).
 *    Action removal should be <= as aggressive (fewer removals but
 *    sound). Any violation indicates a bug.
 *
 * ==========================================================================
 */

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
     * @param ctx Shared Z3 context (passed by reference)
     */
    NumericRelaxedPlanningGraph(const Problem& problem, z3::context& ctx);
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
     * @brief Get indices of actions that can be safely removed (never applicable in any layer)
     */
    std::vector<size_t> get_removable_action_indices() const;

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

    z3::context& ctx_;  // SHARED: reference to external Z3 context
    Z3VariableFactory variable_factory_;  // OWNED: manages Z3 variables per layer
    mutable GroundedEncodingVisitor grounded_visitor_;  // OWNED: converts Expression → z3::expr (mutable for const methods)

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

    static constexpr int WIDENING_THRESHOLD = 3;

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
     * @brief Evaluate an ExprID to an Interval using current layer bounds
     *
     * Walks the expression tree:
     *   - Constants -> point interval [c, c]
     *   - State variables -> lookup in layer's numeric_bounds
     *   - Arithmetic (+, -, *, /) -> interval arithmetic
     *   - Unknown/unsupported -> Interval::unbounded()
     */
    Interval evaluate_interval(ExprID eid, int layer) const;

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
