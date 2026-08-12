#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include "../problem/action.hpp"
#include "../problem/goal.hpp"
#include "../problem/problem.hpp"
#include "../encoders/grounded_encoding_visitor.hpp"
#include "../encoders/z3_variable_factory.hpp"
#include "../problem/visitors/fluent_collector.hpp"
#include "../grounding/interval.hpp"
#include "../util/stats.hpp"
#include <z3++.h>
#include <memory>
#include <optional>
#include <chrono>

namespace rantanplan {

/**
 * @brief AchieversAnalysis
 *
 * Maps each precondition (literal) of actions to the actions that contain it.
 * Uses semantic SMT-based checking to determine which actions can achieve conditions.
 * For each action-condition pair, checks if there exists a state where executing the
 * action transitions the condition from false to true.
 *
 * Incremental updates: across iterations of ActionPruningPass, call update()
 * instead of re-constructing. The analysis caches per-achiever "relevant
 * fluents" (every fluent appearing in the SMT query for that pair) and the
 * previous RPG bounds. On update():
 *   - Pairs that were UNSAT before are skipped (tighter bounds keep UNSAT).
 *   - Pairs whose relevant fluents are not in the changed-bounds set are
 *     adopted directly (the SMT solver's constraints over those fluents are
 *     identical, so the result must be the same).
 *   - Only the remaining pairs (previously SAT, with at least one relevant
 *     fluent whose bounds tightened) require a fresh Z3 query.
 * Action identity across Problem::without_actions is by `action.label()`
 * (name + parameters); IDs are re-indexed and unsafe.
 *
 * Test coverage:
 *   pddl/test/incremental-cascade/   - 2-step cascade: iter 1 removes a
 *     "waste" action, iter 2's update flips an achiever SAT->UNSAT via a
 *     value-expression-driven bound dependency, iter 3 reaches fixpoint.
 *   pddl/test/incremental-cascade-3/ - 3-step cascade: a multi-effect
 *     "trigger" action whose Boolean side keeps a chain alive in iter 2
 *     while its numeric side defers a second bound tightening to iter 3,
 *     producing Z3 re-verifications in BOTH iter 2 and iter 3.
 *
 * TODO: It is quite expensive for big problems. Consider moving the Boolean checks to syntactic checks
 * Also, can we group checks?
 */
class AchieversAnalysis {
public:
    /// Pre-computed RPG data that can be passed to avoid redundant RPG computation.
    struct RPGData {
        std::unordered_map<ExprID, Interval> state_variable_bounds;
        std::unordered_map<int, int> action_first_layers;
        int layer_count = 1;
    };

    // Constructor — runs its own internal NumericRPG
    AchieversAnalysis() = delete;
    explicit AchieversAnalysis(const Problem& problem);

    // Constructor — accepts pre-computed RPG data (avoids redundant RPG computation)
    AchieversAnalysis(const Problem& problem, RPGData rpg_data);

    // Build the analysis from a problem
    void analyze(const Problem& problem);

    /// Incrementally update after action pruning. Compares old bounds
    /// (stored internally) with new bounds to determine which achiever
    /// pairs need Z3 re-verification. Pairs that were UNSAT before are
    /// skipped (bounds only tighten). Pairs whose relevant fluents'
    /// bounds didn't change are adopted directly.
    void update(const Problem& new_problem, RPGData new_rpg_data);

    /**
     * @brief Compute the set of goal-relevant actions via transitive achiever closure.
     *
     * Starting from goal conditions, BFS through achiever chains:
     * each condition's achievers are relevant, and their preconditions
     * are added to the frontier. Optionally includes actions whose
     * effects modify fluents appearing in cost expressions (SDAC safety).
     *
     * @param include_cost_fluents If true, also marks as relevant any action
     *        that modifies a fluent referenced by a cost expression of
     *        any already-relevant action. Ensures optimal planning soundness
     *        under state-dependent action costs.
     * @return Set of action indices (into problem.actions()) that are goal-relevant.
     */
    std::unordered_set<size_t> compute_goal_relevant_action_indices(
        const Problem& problem, bool include_cost_fluents) const;
    
    // Access methods for preconditions
    const std::unordered_set<Action>& get_actions_requiring_precondition(ExprID precondition) const;
    const std::unordered_set<ExprID>& get_preconditions(const Action& action) const;

    // Access methods for achievers
    const std::unordered_set<Action>& get_achievers(ExprID condition) const;
    const std::unordered_set<ExprID>& get_achieved_conditions(const Action& action) const;

    // Get conditions in various shapes/forms
    const std::unordered_set<ExprID>& get_all_conditions() const;
    const std::unordered_set<ExprID>& get_pre_conditions() const;
    const std::unordered_set<ExprID>& get_goal_conditions() const;
    
    // Output method
    void print_analysis() const;

    // Clear the analysis
    void clear();

    // ARPG layer ordering: which layer an action first becomes applicable
    int get_action_first_layer(int action_id) const;
    int get_arpg_num_layers() const { return arpg_num_layers_; }

private:
    // Map from precondition to set of actions that require it
    std::unordered_map<ExprID, std::unordered_set<Action>> precondition_to_actions_;

    // Map from action to set of preconditions it requires
    std::unordered_map<Action, std::unordered_set<ExprID>> action_to_preconditions_;

    // Map from condition to set of actions that can achieve it (using semantic analysis)
    std::unordered_map<ExprID, std::unordered_set<Action>> condition_to_achievers_;

    // Map from action to set of conditions it achieves (using semantic analysis)
    std::unordered_map<Action, std::unordered_set<ExprID>> action_to_achieved_conditions_;

    // Set of goal conditions (extracted from goal expressions)
    std::unordered_set<ExprID> pre_conditions_;
    std::unordered_set<ExprID> goal_conditions_;

    // Cached set of all conditions (computed once for performance)
    mutable std::unordered_set<ExprID> all_conditions_cache_;
    mutable bool all_conditions_cached_ = false;
    
    // SMT solving infrastructure
    std::unique_ptr<z3::context> ctx_;
    std::unique_ptr<Z3VariableFactory> variable_factory_;
    std::unique_ptr<GroundedEncodingVisitor> visitor_;
    std::unique_ptr<z3::solver> persistent_solver_;  // Persistent solver with bounds constraints

    // Problem reference for SMT encoding
    const Problem* problem_;

    // State variable bounds for SMT constraint generation (keyed by ExprID).
    std::unordered_map<ExprID, Interval> state_variable_bounds_;

    // RPG action layer ordering: action_id → first layer where applicable
    std::unordered_map<int, int> action_id_to_first_layer_;
    int arpg_num_layers_ = 1;

    // Statistics tracking
    mutable size_t z3_query_count_ = 0;

    // Incremental analysis state.
    // Per-achiever relevant fluent cache: condition → action_label → fluent ExprIDs
    // in the SMT query. Computed on first run, reused on updates. Stable because
    // all keys are ExprIDs (shared pool) or labels (name+params).
    std::unordered_map<ExprID,
        std::unordered_map<std::string, std::unordered_set<ExprID>>>
        achiever_relevant_fluents_;
    // Fluents whose bounds changed since the previous analysis.
    // Empty on first run; populated by update() before re-analyzing.
    std::unordered_set<ExprID> changed_bound_fluents_;

    // Common initialization logic shared by both constructors
    void initialize_from_rpg_data(const Problem& problem, RPGData rpg_data);

    // Helper methods
    void extract_cnf_literals(ExprID eid, std::vector<ExprID>& literals) const;
    void process_action_preconditions(const Action& action);
    void process_goal_conditions(const Goal& goal);
    void analyze_semantic_achievers();
    std::unordered_set<ExprID> collect_fluents_in_expression(ExprID eid);
    std::unordered_set<ExprID> get_action_modified_fluents(const Action& action);
    // Collect ALL fluents referenced by the SMT query for (action, condition).
    std::unordered_set<ExprID> collect_query_relevant_fluents(
        const Action& action, ExprID condition_eid);
    bool fluent_sets_intersect(const std::unordered_set<ExprID>& set1, const std::unordered_set<ExprID>& set2);
    // SMT solver management methods for push/pop approach
    void initialize_persistent_solver();
    void add_bounds_constraints_to_solver();
    bool check_action_achieves_condition_with_pushpop(const Action& action, ExprID condition_eid);
};

} // namespace rantanplan