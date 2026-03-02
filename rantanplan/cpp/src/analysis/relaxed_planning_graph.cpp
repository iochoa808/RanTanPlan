#include "relaxed_planning_graph.hpp"
#include "../config/config.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>

namespace rantanplan {

// Static member definitions
const std::vector<const Action*> RelaxedPlanningGraph::empty_action_vector_;
const std::unordered_set<int> RelaxedPlanningGraph::empty_condition_set_;

RelaxedPlanningGraph::RelaxedPlanningGraph(const Problem& problem)
    : problem_(problem), build_time_ms_(0.0) {
    extract_goal_conditions();
}

void RelaxedPlanningGraph::reset() {
    fact_layers_.clear();
    action_layers_.clear();
    achievability_layer_.clear();
}


bool RelaxedPlanningGraph::build() {
    auto& config = Config::instance();
    ScopedTimer timer("rpg.boolean.build_time_ms");
    double start_memory = MemoryTracker::instance().get_current_memory_mb();

    reset();
    initialize_fact_layer();

    // Boolean RPG must always run to fixpoint for soundness
    // (it uses syntactic relaxation which is less precise than numeric bounds)
    // Build layers until fixpoint (with safety limit)
    const int MAX_RPG_LAYERS = 100; // Prevent infinite expansion
    while (!is_fixpoint_reached() && fact_layers_.size() < MAX_RPG_LAYERS) {
        int current_layer = fact_layers_.size() - 1;

        // Compute applicable actions
        auto applicable_actions = compute_applicable_actions(current_layer);
        action_layers_.push_back(std::move(applicable_actions));

        // Early termination if no actions are applicable
        if (action_layers_[current_layer].empty()) {
            break; // Fixpoint reached - no new actions can be applied
        }

        // Create next fact layer - copy all facts from current layer
        fact_layers_.emplace_back(fact_layers_[current_layer]);
        int next_layer = fact_layers_.size() - 1;

        // Add effects of applicable actions
        for (const Action* action : action_layers_[current_layer]) {
            add_effects_to_layer(*action, next_layer);
        }

        // Boolean RPG continues to fixpoint - no early termination
        // (early termination is only sound for the more precise Numeric RPG)
    }

    build_time_ms_ = timer.elapsed_ms();
    double end_memory = MemoryTracker::instance().get_current_memory_mb();
    double memory_used = end_memory - start_memory;

    bool goals_reachable = are_goals_achievable();

    // Count reachable actions
    int total_actions = problem_.action_count();
    std::unordered_set<const Action*> reachable_action_set;
    for (const auto& layer : action_layers_) {
        for (const Action* action : layer) {
            reachable_action_set.insert(action);
        }
    }
    int reachable_actions = reachable_action_set.size();
    int removed_actions = total_actions - reachable_actions;

    // Record to Stats
    Stats::instance().set("rpg.boolean.total_layers", fact_layers_.size());
    Stats::instance().set("rpg.boolean.total_actions", total_actions);
    Stats::instance().set("rpg.boolean.reachable_actions", reachable_actions);
    Stats::instance().set("rpg.boolean.removed_actions", removed_actions);
    Stats::instance().set("rpg.boolean.memory_mb", memory_used);
    Stats::instance().set("rpg.boolean.goals_reachable", goals_reachable ? 1.0 : 0.0);

    // Structured visual output
    std::ostringstream actions_str;
    actions_str << total_actions << " (" << reachable_actions << " reachable, " << removed_actions << " removed)";

    Logger::instance().component(VerbosityLevel::INFO, "RPG.Boolean", {
        {"time", std::to_string(static_cast<int>(build_time_ms_)) + "ms"},
        {"layers", std::to_string(fact_layers_.size())},
        {"actions", actions_str.str()},
        {"mem", std::to_string(static_cast<int>(memory_used)) + "MB"},
        {"goals", goals_reachable ? "REACHABLE" : "UNREACHABLE"}
    });

    //print_debug_info();

    return goals_reachable;
}

void RelaxedPlanningGraph::initialize_fact_layer() {
    fact_layers_.emplace_back();
    const auto& pool = problem_.pool();

    // Populate layer 0 from the complete initial state.
    // Each Boolean fluent contributes EITHER a positive OR a negative fact
    // (not both), reflecting whether it starts true or false.  Numeric
    // fluents always contribute a positive fact (their value is irrelevant
    // to the Boolean RPG — we just record that the fluent "exists").
    for (size_t i = 0; i < problem_.initial_assignment_count(); ++i) {
        const auto& assignment = problem_.initial_assignment(i);
        int fluent_id = find_grounded_fluent_id(assignment.fluent_id());

        if (fluent_id == -1) continue;

        ExprID val_eid = assignment.value_id();

        if (pool.is_constant(val_eid) && pool.payload_is_bool(val_eid)) {
            if (pool.payload_bool(val_eid)) {
                // Boolean fluent set to true → positive fact (fluent_id)
                fact_layers_[0].insert(fluent_id);
                achievability_layer_[fluent_id] = 0;
            } else {
                // Boolean fluent set to false → negative fact (-(fluent_id+2))
                // This lets preconditions like NOT(p) be satisfied at layer 0.
                int negative_fluent_id = encode_negative_fact_id(fluent_id);
                fact_layers_[0].insert(negative_fluent_id);
                achievability_layer_[negative_fluent_id] = 0;
            }
        } else {
            // Numeric fluent → always positive fact (value doesn't matter
            // for the Boolean RPG — we only need to know the fluent is "active").
            fact_layers_[0].insert(fluent_id);
            achievability_layer_[fluent_id] = 0;
        }
    }
}

bool RelaxedPlanningGraph::is_achievable(ExprID condition_eid) const {
    int fluent_id = find_grounded_fluent_id(condition_eid);
    return fluent_id != -1 && achievability_layer_.contains(fluent_id);
}

int RelaxedPlanningGraph::get_achievability_layer(ExprID condition_eid) const {
    int fluent_id = find_grounded_fluent_id(condition_eid);
    if (fluent_id == -1) return -1;
    return achievability_layer_.contains(fluent_id) ? achievability_layer_.at(fluent_id) : -1;
}

const std::vector<const Action*>& RelaxedPlanningGraph::get_actions_in_layer(int layer) const {
    return action_layers_[layer];
}

const std::unordered_set<int>& RelaxedPlanningGraph::get_conditions_in_layer(int layer) const {
    return fact_layers_[layer];
}

bool RelaxedPlanningGraph::are_goals_achievable() const {
    // Check Boolean goals
    for (int goal_id : goal_condition_ids_) {
        if (!achievability_layer_.contains(goal_id)) {
            return false;
        }
    }

    // For numeric goals: assume all are achievable (since we can't map them to fluent IDs)
    // This is consistent with the relaxed planning graph optimistic approach
    return true;
}

void RelaxedPlanningGraph::extract_goal_conditions() {
    goal_condition_ids_.clear();
    for (size_t i = 0; i < problem_.goal_count(); ++i) {
        std::vector<ExprID> conditions;
        extract_cnf_conditions(problem_.goal(i).goal_id(), conditions);

        // Convert ExprIDs to grounded fluent IDs
        for (ExprID condition_eid : conditions) {
            int goal_id = find_grounded_fluent_id(condition_eid);
            if (goal_id != -1) {
                goal_condition_ids_.push_back(goal_id);
            }
            // Note: Numeric conditions (like >= comparisons) are not stored as goal IDs
            // since they can't be mapped to grounded fluents. RPG only handles Boolean goals.
        }
    }
}

void RelaxedPlanningGraph::extract_cnf_conditions(ExprID eid, std::vector<ExprID>& conditions) const {
    const auto& pool = problem_.pool();
    if (pool.is_and(eid)) {
        for (ExprID arg : pool.arguments(eid)) {
            extract_cnf_conditions(arg, conditions);
        }
    } else {
        // Filter out both FLUENT_SYMBOL and FUNCTION_SYMBOL leaves — only emit
        // actual condition literals (STATE_VARIABLE, NOT, comparisons, etc.)
        if (!pool.is_fluent_symbol(eid) && !pool.is_function_symbol(eid)) {
            conditions.push_back(eid);
        }
    }
}

std::vector<const Action*> RelaxedPlanningGraph::compute_applicable_actions(int layer_index) const {
    std::vector<const Action*> applicable;
    for (const auto & action : problem_.actions()) {
        if (are_preconditions_satisfied(action, layer_index)) {
            //     std::cout << "Action " << action.name() << ": " << (satisfied ? "APPLICABLE" : "NOT APPLICABLE") << std::endl;
            applicable.push_back(&action);
        }
    }
    return applicable;
}

bool RelaxedPlanningGraph::are_preconditions_satisfied(const Action& action, int layer_index) const {
    if (!action.has_precondition()) {
        return true;
    }

    std::vector<ExprID> preconditions;
    extract_cnf_conditions(action.precondition_id(), preconditions);

    for (ExprID precond_eid : preconditions) {
        if (!is_condition_satisfied(precond_eid, layer_index)) {
            return false;
        }
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Condition satisfaction checking — the core of positive/negative handling.
//
// Preconditions arrive in CNF form (see cnf_condition_compiler.py).  The
// top-level AND has already been split by extract_cnf_conditions(), so each
// call here receives one clause, which may be:
//
//   (a) A positive Boolean literal          →  look up positive fact ID
//   (b) NOT(Boolean literal)                →  look up negative fact ID
//   (c) EQUALS(c1, c2)                      →  syntactic constant comparison
//   (d) NOT(EQUALS(c1, c2))                 →  negated constant comparison
//   (e) OR(l1, l2, ...)  (CNF clause)       →  true if any disjunct satisfied
//   (f) AND(l1, l2, ...)                    →  all conjuncts must hold
//   (g) Numeric comparison (>=, <=, >, <)   →  assume true (sound relaxation)
//   (h) NOT(AND(...)), NOT(OR(...))         →  assume true (sound relaxation)
//
// The guiding principle: returning "true" (satisfiable) is always safe in
// an RPG because it only keeps actions alive.  Returning "false" must be
// provably correct — otherwise we'd incorrectly prune a reachable action.
// ═══════════════════════════════════════════════════════════════════════════
bool RelaxedPlanningGraph::is_condition_satisfied(ExprID condition_eid, int layer_index) const {
    const auto& pool = problem_.pool();

    // --- Case (c): EQUALS(a, b) between ground terms ---
    // For two constants, ExprPool interning gives same ExprID iff identical.
    // For non-constant operands (state variables), assume true.
    if (pool.is_equals(condition_eid)) {
        return evaluate_ground_equality(condition_eid);
    }

    // --- Cases (b), (d), (h): NOT(...) ---
    if (pool.is_not(condition_eid)) {
        ExprID inner = get_inner_condition(condition_eid);
        if (!inner.valid()) return true; // Can't evaluate — assume satisfiable

        // Case (d): NOT(EQUALS(const, const)) — negate the syntactic check
        if (pool.is_equals(inner)) {
            return !evaluate_ground_equality(inner);
        }

        // NOT(NOT(x)) — double negation elimination → recurse on x
        if (pool.is_not(inner)) {
            ExprID double_neg = get_inner_condition(inner);
            if (double_neg.valid()) {
                return is_condition_satisfied(double_neg, layer_index);
            }
            return true;
        }

        // Case (h): NOT(AND(...)) ≡ OR(NOT(...)) — a disjunction of negations.
        // At least one negation might hold, so assume true (sound relaxation;
        // precisely evaluating would require checking each negated conjunct).
        if (pool.is_and(inner)) {
            return true;
        }

        // Case (h): NOT(OR(...)) ≡ AND(NOT(...)) — a conjunction of negations.
        // We'd need all inner literals to be false; assume true (sound relaxation).
        if (pool.is_or(inner)) {
            return true;
        }

        // Case (b): NOT(fluent) — a negated Boolean literal.
        // find_grounded_fluent_id recognises the NOT wrapper and returns the
        // *negative* fact ID (-(fluent_id+2)).  We then check whether that
        // negative fact ID exists in the current layer.
        if (pool.is_state_variable(inner) || problem_.is_bool_type(inner)) {
            int fid = find_grounded_fluent_id(condition_eid);  // returns negative fact ID
            return fid != -1 && fact_layers_[layer_index].contains(fid);
        }

        // NOT(numeric comparison) or other unrecognised form — assume satisfiable
        return true;
    }

    // --- Case (e): OR (disjunction from CNF / quantifier expansion) ---
    // A CNF clause like (p ∨ ¬q ∨ r).  True if any disjunct is satisfied.
    // If none is provably satisfied, we still return true because the RPG
    // must not falsely prune actions (soundness > precision).
    if (pool.is_or(condition_eid)) {
        const auto& args = pool.has_head_and_arguments(condition_eid)
                           ? pool.arguments(condition_eid)
                           : pool.children(condition_eid);
        for (ExprID arg : args) {
            if (is_condition_satisfied(arg, layer_index)) {
                return true;
            }
        }
        // No disjunct provably satisfied — conservatively assume satisfiable
        // (returning false could prune a genuinely reachable action).
        return true;
    }

    // --- Case (f): AND (conjunction) ---
    // Rarely appears as a nested node (most ANDs are split by extract_cnf_conditions),
    // but can occur inside an OR clause or when CNF expansion is disabled.
    if (pool.is_and(condition_eid)) {
        const auto& args = pool.has_head_and_arguments(condition_eid)
                           ? pool.arguments(condition_eid)
                           : pool.children(condition_eid);
        for (ExprID arg : args) {
            if (!is_condition_satisfied(arg, layer_index)) {
                return false;
            }
        }
        return true;
    }

    // --- Case (a): Positive Boolean literal (ground state variable) ---
    // find_grounded_fluent_id returns the positive fact ID, and we check
    // whether it appears in the current layer.
    if (pool.is_state_variable(condition_eid) || problem_.is_bool_type(condition_eid)) {
        return is_fact_in_layer(condition_eid, layer_index);
    }

    // --- Case (g): Anything else (numeric comparisons, IMPLIES, etc.) ---
    // The Boolean RPG cannot evaluate these, so assume satisfiable.
    return true;
}

// Evaluate an equality expression syntactically.
// With ExprPool interning, two constants are identical iff they share the same
// ExprID.  For non-constant operands (state variables, function applications
// like (= (xl ?l) (x)) or (= (fuel plane1) 0)), the equality depends on
// runtime values that the RPG doesn't track — returning true is sound because
// it keeps the action alive rather than incorrectly pruning it.
bool RelaxedPlanningGraph::evaluate_ground_equality(ExprID equals_eid) const {
    const auto& pool = problem_.pool();
    const auto& args = pool.has_head_and_arguments(equals_eid)
                       ? pool.arguments(equals_eid)
                       : pool.children(equals_eid);
    if (args.size() != 2) return true; // Malformed — assume true (sound)

    if (!pool.is_constant(args[0]) || !pool.is_constant(args[1])) {
        return true; // Non-constant operands — assume satisfiable (sound relaxation)
    }
    return args[0] == args[1];  // Same ExprID ⟺ same constant
}

void RelaxedPlanningGraph::add_effects_to_layer(const Action& action, int target_layer_index) {
    for (size_t i = 0; i < action.effect_count(); ++i) {
        const Effect& effect = action.effect(i);
        const EffectExpression& eff_expr = effect.effect_expression();

        // For conditional and quantified effects in RPG: check conditions before applying
        if (eff_expr.is_conditional()) {
            std::vector<ExprID> conditions;
            extract_cnf_conditions(eff_expr.condition_id(), conditions);

            // Check if all conditions are satisfied in the previous layer
            bool all_conditions_satisfied = true;
            for (ExprID cond_eid : conditions) {
                if (!is_condition_satisfied(cond_eid, target_layer_index - 1)) {
                    all_conditions_satisfied = false;
                    break;
                }
            }

            // Only apply the effect if all conditions are satisfied
            if (all_conditions_satisfied) {
                add_simple_effect_to_layer(effect, target_layer_index);
            }
        } else {
            // Handle simple atomic effects as before
            add_simple_effect_to_layer(effect, target_layer_index);
        }
    }
}

// Apply a single atomic effect to the target layer.
//
// Boolean effects produce either a positive or negative fact:
//   (assign fluent true)  → inserts positive fact ID (fluent_id)
//   (assign fluent false) → inserts negative fact ID (-(fluent_id+2))
//
// This is the key mechanism that makes negated preconditions work in the RPG:
// an action whose effect sets fluent to false creates a negative fact, which
// can then satisfy a NOT(fluent) precondition of another action in a later
// layer.  Both the positive and negative facts coexist monotonically — the
// RPG never retracts a previously added fact.
//
// Numeric effects just insert the positive fluent ID (acting as a
// "this fluent has been modified" marker).
void RelaxedPlanningGraph::add_simple_effect_to_layer(const Effect& effect, int target_layer_index) {
    const EffectExpression& eff_expr = effect.effect_expression();
    const auto& pool = problem_.pool();
    ExprID fluent_eid = eff_expr.fluent_id();
    ExprID val_eid = eff_expr.value_id();

    if (pool.is_constant(val_eid) && pool.payload_is_bool(val_eid)) {
        int fluent_id = find_grounded_fluent_id(fluent_eid);
        assert(fluent_id != -1 && "Effect fluent not recognised");
        if (pool.payload_bool(val_eid)) {
            // Positive Boolean effect: (assign fluent true) → positive fact
            fact_layers_[target_layer_index].insert(fluent_id);
            if (achievability_layer_.find(fluent_id) == achievability_layer_.end()) {
                achievability_layer_[fluent_id] = target_layer_index;
            }
        } else {
            // Negative Boolean effect: (assign fluent false) → negative fact
            // This enables NOT(fluent) preconditions in subsequent layers.
            int negative_fluent_id = encode_negative_fact_id(fluent_id);
            fact_layers_[target_layer_index].insert(negative_fluent_id);
            if (achievability_layer_.find(negative_fluent_id) == achievability_layer_.end()) {
                achievability_layer_[negative_fluent_id] = target_layer_index;
            }
        }
    } else {
        // Numeric effect — record that this fluent has been modified.
        // The RPG doesn't track numeric values; it only cares that the
        // fluent is "active" so numeric preconditions are assumed satisfiable.
        int fluent_id = find_grounded_fluent_id(fluent_eid);
        assert(fluent_id != -1 && "Effect fluent not recognised");
        fact_layers_[target_layer_index].insert(fluent_id);
        if (achievability_layer_.find(fluent_id) == achievability_layer_.end()) {
            achievability_layer_[fluent_id] = target_layer_index;
        }
    }
}

bool RelaxedPlanningGraph::is_fixpoint_reached() const {
    if (fact_layers_.size() < 2) {
        return false;
    }

    // Check if last layer added any new facts (much simpler with IDs!)
    const auto& last_layer = fact_layers_.back();
    const auto& prev_layer = fact_layers_[fact_layers_.size() - 2];

    for (int fact_id : last_layer) {
        if (prev_layer.find(fact_id) == prev_layer.end()) {
            return false; // Found new fact
        }
    }

    return true; // No new facts added
}






// Map an ExprID to an integer fact ID used in the fact layers.
//
// For a positive expression (state variable like `at(robot, city1)`):
//   → returns the fluent's grounded index (0, 1, 2, ...)
//
// For a negated expression (`NOT(at(robot, city1))`):
//   → strips the NOT wrapper, looks up the inner fluent's grounded index,
//     and returns the negative encoding: -(fluent_id + 2) = -2, -3, -4, ...
//
// Returns -1 if the expression doesn't correspond to any known grounded fluent.
int RelaxedPlanningGraph::find_grounded_fluent_id(ExprID eid) const {
    const auto& pool = problem_.pool();
    if (pool.is_not(eid)) {
        // Negated literal: unwrap NOT, find the positive fluent, encode as negative
        ExprID inner_eid = get_inner_condition(eid);
        int fluent_id = problem_.find_grounded_fluent_index(inner_eid);
        return (fluent_id != -1) ? encode_negative_fact_id(fluent_id) : -1;
    }
    // Positive literal: direct lookup
    return problem_.find_grounded_fluent_index(eid);
}


ExprID RelaxedPlanningGraph::get_inner_condition(ExprID negated_eid) const {
    const auto& pool = problem_.pool();
    if (pool.has_head_and_arguments(negated_eid) && pool.argument_count(negated_eid) >= 1) {
        return pool.argument(negated_eid, 0);
    }

    const auto& kids = pool.children(negated_eid);
    if (!kids.empty()) {
        return kids[0];
    }

    return EXPR_NULL;
}

bool RelaxedPlanningGraph::is_fact_in_layer(ExprID condition_eid, int layer_index) const {
    int fluent_id = find_grounded_fluent_id(condition_eid);
    return fluent_id != -1 && fact_layers_[layer_index].contains(fluent_id);
}

int RelaxedPlanningGraph::get_minimum_steps_lower_bound() const {
    // Goals not achievable or RPG not built yet
    if (!are_goals_achievable() || fact_layers_.empty()) {
        return -1;
    }

    // Find the maximum layer where any goal condition first becomes achievable
    int max_goal_layer = 0;
    for (int goal_id : goal_condition_ids_) {
        auto it = achievability_layer_.find(goal_id);
        if (it != achievability_layer_.end()) {
            max_goal_layer = std::max(max_goal_layer, it->second);
        }
    }

    // Layer N means N steps are needed. i.e. Layer 0 = 0 steps, Layer 1 = 1 step, etc.
    return max_goal_layer;
}

std::vector<size_t> RelaxedPlanningGraph::get_removable_action_indices() const {
    // Collect all actions that appear in any layer of the RPG
    std::unordered_set<const Action*> reachable_actions;
    for (const auto& layer : action_layers_) {
        for (const Action* action : layer) {
            reachable_actions.insert(action);
        }
    }

    // Find actions that never appear in any layer (safe to remove after fixpoint)
    std::vector<size_t> indices;
    for (size_t i = 0; i < problem_.action_count(); ++i) {
        if (!reachable_actions.count(&problem_.action(i))) {
            indices.push_back(i);
        }
    }

    return indices;
}


void RelaxedPlanningGraph::print_debug_info() const {
    std::cout << "\n=== Relaxed Planning Graph Debug Info ===\n";
    std::cout << "Total layers: " << fact_layers_.size() << "\n";
    std::cout << "Goals achievable: " << (are_goals_achievable() ? "YES" : "NO") << "\n\n";

    for (size_t i = 0; i < fact_layers_.size(); ++i) {
        std::cout << "--- Layer " << i << " ---\n";
        std::cout << "Facts (" << fact_layers_[i].size() << "):\n";
        for (int fact_id : fact_layers_[i]) {
            std::cout << "  " << fact_id_to_string(fact_id) << "\n";
        }

        if (i < action_layers_.size()) {
            std::cout << "Actions (" << action_layers_[i].size() << "):\n";
            for (const Action* action : action_layers_[i]) {
                std::cout << "  " << action->name() << "\n";
            }
        }
        std::cout << "\n";
    }

    std::cout << "Goal conditions:\n";
    for (int goal_id : goal_condition_ids_) {
        auto it = achievability_layer_.find(goal_id);
        int layer = (it != achievability_layer_.end()) ? it->second : -1;
        std::cout << "  " << fact_id_to_string(goal_id) << " -> layer " << layer << "\n";
    }


    std::cout << "=========================================\n\n";
}

void RelaxedPlanningGraph::print_reachability_analysis() const {
    std::cout << "\n=== RPG Reachability Analysis ===\n";

    // Collect all reached fluent IDs from all layers
    std::unordered_set<int> all_reached_fluents;
    for (const auto& layer : fact_layers_) {
        for (int fact_id : layer) {
            // Only count positive fluent IDs (negative are encoded negations)
            if (fact_id >= 0) {
                all_reached_fluents.insert(fact_id);
            }
        }
    }

    // Collect all reached actions from all layers
    std::unordered_set<const Action*> all_reached_actions;
    for (const auto& layer : action_layers_) {
        for (const Action* action : layer) {
            all_reached_actions.insert(action);
        }
    }

    // Total counts
    size_t total_grounded_fluents = problem_.grounded_fluent_count();
    size_t total_actions = problem_.action_count();

    // Reachable counts
    size_t reachable_fluents = all_reached_fluents.size();
    size_t reachable_actions = all_reached_actions.size();

    // Calculate unreachable counts
    size_t unreachable_fluents = total_grounded_fluents - reachable_fluents;
    size_t unreachable_actions = total_actions - reachable_actions;

    std::cout << "Fluent Analysis:\n";
    std::cout << "  Total grounded fluents: " << total_grounded_fluents << "\n";
    std::cout << "  Reachable fluents: " << reachable_fluents << "\n";
    std::cout << "  Unreachable fluents: " << unreachable_fluents << "\n";
    if (total_grounded_fluents > 0) {
        double fluent_coverage = (double)reachable_fluents / total_grounded_fluents * 100.0;
        std::cout << "  Fluent coverage: " << std::fixed << std::setprecision(1) << fluent_coverage << "%\n";
    }

    std::cout << "\nAction Analysis:\n";
    std::cout << "  Total actions: " << total_actions << "\n";
    std::cout << "  Reachable actions: " << reachable_actions << "\n";
    std::cout << "  Unreachable actions: " << unreachable_actions << "\n";
    if (total_actions > 0) {
        double action_coverage = (double)reachable_actions / total_actions * 100.0;
        std::cout << "  Action coverage: " << std::fixed << std::setprecision(1) << action_coverage << "%\n";
    }

    // Show removable actions (safe to remove after fixpoint analysis)
    auto removable_indices = get_removable_action_indices();
    if (!removable_indices.empty()) {
        double savings_percentage = (double)removable_indices.size() / total_actions * 100.0;
        std::cout << "  Removable actions: " << removable_indices.size() << " ("
                  << std::fixed << std::setprecision(1) << savings_percentage << "% potential SAT encoding savings)\n";
    }


    // List unreachable fluents (up to 10 for brevity)
    if (unreachable_fluents > 0) {
        std::cout << "\nUnreachable fluents (showing up to 10):\n";
        int count = 0;
        for (size_t i = 0; i < total_grounded_fluents && count < 10; ++i) {
            if (all_reached_fluents.find(static_cast<int>(i)) == all_reached_fluents.end()) {
                std::cout << "  " << problem_.pool().to_string(problem_.grounded_fluent(i)) << "\n";
                count++;
            }
        }
        if (unreachable_fluents > 10) {
            std::cout << "  ... and " << (unreachable_fluents - 10) << " more\n";
        }
    }

    // List unreachable actions (up to 10 for brevity)
    if (unreachable_actions > 0) {
        std::cout << "\nUnreachable actions (showing up to 10):\n";
        int count = 0;
        for (size_t i = 0; i < total_actions && count < 10; ++i) {
            const Action& action = problem_.action(i);
            if (all_reached_actions.find(&action) == all_reached_actions.end()) {
                std::cout << "  " << action.name() << "\n";
                count++;
            }
        }
        if (unreachable_actions > 10) {
            std::cout << "  ... and " << (unreachable_actions - 10) << " more\n";
        }
    }

    std::cout << "==================================\n\n";
}

std::string RelaxedPlanningGraph::fact_id_to_string(int fact_id) const {
    if (fact_id >= 0) {
        // Positive fact
        if (fact_id < static_cast<int>(problem_.grounded_fluent_count())) {
            return problem_.pool().to_string(problem_.grounded_fluent(fact_id));
        }
    } else {
        // Negative fact: decode to get original fluent_id
        int original_fluent_id = decode_negative_fact_id(fact_id);
        if (original_fluent_id >= 0 && original_fluent_id < static_cast<int>(problem_.grounded_fluent_count())) {
            return "(not " + problem_.pool().to_string(problem_.grounded_fluent(original_fluent_id)) + ")";
        }
    }
    return "unknown_fact_" + std::to_string(fact_id);
}



} // namespace rantanplan