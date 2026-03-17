#pragma once

#include "../problem/problem.hpp"
#include "../problem/action.hpp"
#include "../problem/goal.hpp"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <chrono>

namespace rantanplan {

/**
 * @brief RelaxedPlanningGraph — Boolean-only monotone reachability analysis.
 *
 * NOTE: Currently unused. NumericRelaxedPlanningGraph is a strict superset
 * (handles both boolean and numeric fluents) and is used by default via
 * NumericRPGPass. This class is kept for potential lightweight/fast-path use.
 *
 * Layer-by-layer relaxed planning graph implementation using traditional fixpoint computation.
 *
 * ## Positive and Negative Literal Handling
 *
 * Unlike a classical delete-relaxation (which only tracks positive facts and ignores
 * delete effects), this RPG tracks BOTH positive and negative facts. This is necessary
 * because grounded preconditions can require negative literals, e.g. NOT(at(robot, city1)).
 *
 * **Fact ID encoding:**
 *   - Positive facts use the fluent's grounded index directly: 0, 1, 2, ...
 *   - Negative facts use `encode_negative_fact_id(fluent_id) = -(fluent_id + 2)`: -2, -3, -4, ...
 *   - The value -1 is reserved as the "not found" sentinel.
 *
 * **Monotonicity (the relaxation):**
 *   Both positive and negative facts accumulate monotonically across layers. Once a fact
 *   is added, it persists in all subsequent layers (each new layer starts as a copy of the
 *   previous one). A fluent can be simultaneously positive AND negative in the same layer
 *   — e.g., both `at(robot, city1)` and `NOT(at(robot, city1))` can coexist. This is the
 *   relaxation: we never retract a fact, so we over-approximate what's reachable.
 *
 * **Initial state:** Boolean fluents assigned true produce positive facts; those assigned
 *   false produce negative facts. Numeric fluents always produce positive facts.
 *
 * **Effects:** An action with `(assign fluent true)` adds the positive fact; `(assign
 *   fluent false)` adds the negative fact. Numeric effects add the fluent as positive.
 *
 * **Precondition checking (`is_condition_satisfied`):**
 *   - Positive Boolean literal `p` → look up positive fact ID in the layer.
 *   - Negated Boolean literal `NOT(p)` → look up negative fact ID in the layer.
 *   - `EQUALS(c1, c2)` between constants → syntactic comparison via ExprID interning.
 *   - `NOT(EQUALS(...))` → negate the equality result.
 *   - `OR(...)` (from CNF / quantifier expansion) → true if any disjunct satisfied;
 *     if none is provably satisfied, assume true (sound over-approximation).
 *   - `AND(...)` → all conjuncts must be satisfied.
 *   - Numeric comparisons, `NOT(AND)`, `NOT(OR)` → assume true (sound relaxation).
 *
 * **Soundness guarantee:** Since the RPG only over-approximates reachability, any action
 *   it deems unreachable is genuinely unreachable and safe to remove. The only risk is
 *   keeping actions that turn out to be unreachable (not harmful, just less pruning).
 *
 * ## Other Features
 *   - Simplified numeric handling: any modification of a numeric variable enables any
 *     condition referencing it.
 *   - Always runs to fixpoint (no early termination) for soundness.
 *   - Independent from ARPG and current achievers analysis.
 *
 * Note: The more precise NumericRelaxedPlanningGraph can use early termination safely.
 */
class RelaxedPlanningGraph {
public:
    explicit RelaxedPlanningGraph(const Problem& problem);

    /**
     * Build the relaxed planning graph from initial state until fixpoint.
     * @return true if all goals are reachable, false otherwise
     */
    bool build();

    /**
     * Check if a specific condition expression is achievable.
     * @param condition The condition to check (can be Boolean or numeric)
     * @return true if the condition is achievable in the relaxed graph
     */
    bool is_achievable(ExprID condition_eid) const;

    /**
     * Get the first layer where a condition becomes achievable.
     * @param condition_eid The condition ExprID to check
     * @return layer number (0-based), or -1 if not achievable
     */
    int get_achievability_layer(ExprID condition_eid) const;

    /**
     * Get all actions that become applicable in a given layer.
     * @param layer The layer number (0-based)
     * @return reference to vector of actions, empty if layer doesn't exist
     */
    const std::vector<const Action*>& get_actions_in_layer(int layer) const;

    /**
     * Get all conditions that become true in a given layer.
     * @param layer The layer number (0-based)
     * @return reference to set of condition IDs, empty if layer doesn't exist
     */
    const std::unordered_set<int>& get_conditions_in_layer(int layer) const;

    /**
     * Get the total number of layers in the graph.
     * @return number of layers, 0 if graph hasn't been built
     */
    size_t get_layer_count() const { return fact_layers_.size(); }

    /**
     * Check if all goal conditions are achievable.
     * @return true if all goals are reachable
     */
    bool are_goals_achievable() const;

    /**
     * Reset the graph for recomputation.
     */
    void reset();

    /**
     * Print debugging information about the graph structure.
     */
    void print_debug_info() const;

    /**
     * Print reachability analysis comparing reached vs total fluents/actions.
     */
    void print_reachability_analysis() const;

    /**
     * Get the minimum number of steps (transitions) needed to achieve the goals.
     * This provides a lower bound for planning based on the RPG structure.
     * @return minimum steps needed, or -1 if goals are not achievable
     * Note: 1 layer = 0 steps, 2 layers = 1 step, etc.
     */
    int get_minimum_steps_lower_bound() const;

    /**
     * Get indices of actions that never appear in any RPG layer after fixpoint analysis.
     * These actions are safe to remove because:
     * - Actions with only numeric preconditions would appear immediately
     * - Actions that never appear have unsatisfiable Boolean preconditions
     * @return vector of action indices that never become applicable
     */
    std::vector<size_t> get_removable_action_indices() const;


private:
    const Problem& problem_;

    // Layer-based storage using grounded fluent IDs for efficiency.
    // Fact encoding (see encode_negative_fact_id / decode_negative_fact_id):
    //   positive facts → fluent_id directly:          0,  1,  2, ...
    //   negative facts → -(fluent_id + 2):           -2, -3, -4, ...
    //   -1 is the "not found" sentinel (never stored in a layer).
    // Both positive and negative facts grow monotonically: once added to a
    // layer, they persist in all subsequent layers (the delete-relaxation).
    std::vector<std::unordered_set<int>> fact_layers_;                   // IDs of facts (positive and negative) true at each layer
    std::vector<std::vector<const Action*>> action_layers_;              // Pointers to actions applicable at each layer

    // Achievability tracking — same fact-ID encoding as fact_layers_.
    // Records the earliest layer at which each fact first becomes true.
    std::unordered_map<int, int> achievability_layer_;                   // Maps fact_id -> first layer it's achievable


    // Goal tracking - we'll extract condition IDs during goal processing
    std::vector<int> goal_condition_ids_;                               // IDs of goal conditions

    // Timing and statistics
    mutable std::chrono::high_resolution_clock::time_point build_start_time_;
    mutable double build_time_ms_;

    // Static empty containers for safe returns
    static const std::vector<const Action*> empty_action_vector_;
    static const std::unordered_set<int> empty_condition_set_;

    /**
     * Initialize the first fact layer with the initial state.
     */
    void initialize_fact_layer();

    /**
     * Extract all goal conditions from the problem goals.
     */
    void extract_goal_conditions();

    /**
     * Extract conditions from CNF expressions (based on AchieversAnalysis pattern).
     * @param expr The expression to extract from
     * @param conditions Output vector for pointers to extracted conditions
     */
    void extract_cnf_conditions(ExprID eid, std::vector<ExprID>& conditions) const;

    /**
     * Compute which actions become applicable at the current layer.
     * @param layer_index The current layer index
     * @return vector of newly applicable actions
     */
    std::vector<const Action*> compute_applicable_actions(int layer_index) const;

    /**
     * Check if all preconditions of an action are satisfied at a given layer.
     * @param action The action to check
     * @param layer_index The layer to check against
     * @return true if all preconditions are satisfied
     */
    bool are_preconditions_satisfied(const Action& action, int layer_index) const;

    /**
     * Check if a single condition is satisfied at a given layer.
     * Handles positive/negated fluent conditions, equality between constants,
     * NOT(equality), and OR clauses produced by quantifier removal + CNF.
     * @param condition The condition to check (may be negated)
     * @param layer_index The layer to check against
     * @return true if the condition is satisfied
     */
    bool is_condition_satisfied(ExprID condition_eid, int layer_index) const;

    /**
     * Evaluate a ground equality expression between constants.
     * Uses ExprPool interning: same ExprID ⟹ structurally identical.
     * @param equals_eid An ExprID whose operator is EQUALS
     * @return true if the two arguments are identical constants
     */
    bool evaluate_ground_equality(ExprID equals_eid) const;

    /**
     * Add effects of an action to the next fact layer.
     * @param action The action whose effects to add
     * @param target_layer_index The layer index to add effects to
     */
    void add_effects_to_layer(const Action& action, int target_layer_index);

    /**
     * Add a simple atomic effect to the target layer.
     * @param effect The simple effect to add
     * @param target_layer_index The layer index to add effect to
     */
    void add_simple_effect_to_layer(const Effect& effect, int target_layer_index);


    /**
     * Check if a fixpoint has been reached (no new facts in the last layer).
     * @return true if no progress was made in the last iteration
     */
    bool is_fixpoint_reached() const;

    /**
     * Check if a fact (positive or negative) exists in the given layer.
     * @param condition The condition to check (can be positive or negative)
     * @param layer_index The layer to check against
     * @return true if the fact exists in the layer
     */
    bool is_fact_in_layer(ExprID condition_eid, int layer_index) const;


    /**
     * Extract the inner condition from a negated condition.
     * @param negated_eid The negated condition ExprID (must be negated)
     * @return ExprID of the inner positive condition
     */
    ExprID get_inner_condition(ExprID negated_eid) const;


    /**
     * Find the grounded fluent ID for a given ExprID.
     * For positive expressions: returns fluent_id (0, 1, 2, ...)
     * For negative expressions: returns -(fluent_id + 1) (-1, -2, -3, ...)
     * @param eid The ExprID to find fluent ID for (can be positive or negative)
     * @return encoded fact ID, or -1 if not found
     */
    int find_grounded_fluent_id(ExprID eid) const;

    /**
     * Helper to decode fact ID and get string representation for debugging.
     * @param fact_id The encoded fact ID
     * @return string representation of the fact (with "not" prefix for negative facts)
     */
    std::string fact_id_to_string(int fact_id) const;

    /**
     * Encode a positive fluent ID as a negative fact ID.
     * Uses -(fluent_id + 2) to avoid collision with -1 sentinel (not found).
     * @param fluent_id The positive fluent ID (0, 1, 2, ...)
     * @return negative fact ID (-2, -3, -4, ...)
     */
    static int encode_negative_fact_id(int fluent_id) { return -(fluent_id + 2); }

    /**
     * Decode a negative fact ID to get the original positive fluent ID.
     * @param negative_fact_id The negative fact ID (-2, -3, -4, ...)
     * @return original positive fluent ID
     */
    static int decode_negative_fact_id(int negative_fact_id) { return -(negative_fact_id + 2); }
};

} // namespace rantanplan