#include "relaxed_planning_graph.h"
#include <iostream>
#include <iomanip>

namespace planmt {

// Static member definitions
const std::vector<const Action*> RelaxedPlanningGraph::empty_action_vector_;
const std::unordered_set<int> RelaxedPlanningGraph::empty_condition_set_;

RelaxedPlanningGraph::RelaxedPlanningGraph(const Problem& problem)
    : problem_(problem) {
    extract_goal_conditions();
    analyze_numeric_modifications();
}

bool RelaxedPlanningGraph::build() {
    reset();
    initialize_fact_layer();

    // Build layers until fixpoint
    while (!is_fixpoint_reached()) {
        int current_layer = fact_layers_.size() - 1;

        // Compute applicable actions
        auto applicable_actions = compute_applicable_actions(current_layer);
        action_layers_.push_back(std::move(applicable_actions));

        // Create next fact layer - copy all facts from current layer (no delete effects in RPG)
        fact_layers_.emplace_back(fact_layers_[current_layer]);
        int next_layer = fact_layers_.size() - 1;

        // Add effects of applicable actions
        for (const Action* action : action_layers_[current_layer]) {
            add_effects_to_layer(*action, next_layer);
        }
    }

    return are_goals_achievable();
}

bool RelaxedPlanningGraph::is_achievable(const Expression& condition) const {
    int fluent_id = find_grounded_fluent_id(condition);
    if (fluent_id == -1) {
        return false;
    }
    return achievability_layer_.find(fluent_id) != achievability_layer_.end();
}

int RelaxedPlanningGraph::get_achievability_layer(const Expression& condition) const {
    int fluent_id = find_grounded_fluent_id(condition);
    if (fluent_id == -1) {
        return -1;
    }

    auto it = achievability_layer_.find(fluent_id);
    return it != achievability_layer_.end() ? it->second : -1;
}

const std::vector<const Action*>& RelaxedPlanningGraph::get_actions_in_layer(int layer) const {
    return (layer >= 0 && layer < action_layers_.size()) ? action_layers_[layer] : empty_action_vector_;
}

const std::unordered_set<int>& RelaxedPlanningGraph::get_conditions_in_layer(int layer) const {
    return (layer >= 0 && layer < fact_layers_.size()) ? fact_layers_[layer] : empty_condition_set_;
}

bool RelaxedPlanningGraph::are_goals_achievable() const {
    for (int goal_id : goal_condition_ids_) {
        if (achievability_layer_.find(goal_id) == achievability_layer_.end()) {
            return false;
        }
    }
    return true;
}

void RelaxedPlanningGraph::reset() {
    fact_layers_.clear();
    action_layers_.clear();
    achievability_layer_.clear();
}

void RelaxedPlanningGraph::print_debug_info() const {
    std::cout << "\n=== Relaxed Planning Graph Debug Info ===\n";
    std::cout << "Total layers: " << fact_layers_.size() << "\n";
    std::cout << "Goals achievable: " << (are_goals_achievable() ? "YES" : "NO") << "\n\n";

    for (size_t i = 0; i < fact_layers_.size(); ++i) {
        std::cout << "--- Layer " << i << " ---\n";
        std::cout << "Facts (" << fact_layers_[i].size() << "):\n";
        for (int fact_id : fact_layers_[i]) {
            if (fact_id >= 0 && fact_id < problem_.grounded_fluent_count()) {
                std::cout << "  " << problem_.grounded_fluent(fact_id).to_string() << "\n";
            }
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
        if (goal_id >= 0 && goal_id < problem_.grounded_fluent_count()) {
            std::cout << "  " << problem_.grounded_fluent(goal_id).to_string() << " -> layer " << layer << "\n";
        }
    }

    std::cout << "Modified numeric fluents: " << modified_numeric_fluents_.size() << "\n";
    std::cout << "=========================================\n\n";
}

void RelaxedPlanningGraph::initialize_fact_layer() {
    fact_layers_.emplace_back();

    // Add initial state facts
    for (size_t i = 0; i < problem_.initial_assignment_count(); ++i) {
        const auto& assignment = problem_.initial_assignment(i);
        const Expression& fluent = assignment.fluent();

        // Only add positive Boolean facts and numeric assignments
        if (assignment.value().is_atom() && assignment.value().value().is_boolean() && assignment.value().value().boolean()) {
            int fluent_id = find_grounded_fluent_id(fluent);
            if (fluent_id != -1) {
                fact_layers_[0].insert(fluent_id);
                achievability_layer_[fluent_id] = 0;
            }
        } else if (assignment.value().is_atom() && !assignment.value().value().is_boolean()) {
            // For numeric fluents, store the assignment itself as a fact
            int fluent_id = find_grounded_fluent_id(fluent);
            if (fluent_id != -1) {
                fact_layers_[0].insert(fluent_id);
                achievability_layer_[fluent_id] = 0;
            }
        }
    }
}

void RelaxedPlanningGraph::extract_goal_conditions() {
    goal_condition_ids_.clear();
    for (size_t i = 0; i < problem_.goal_count(); ++i) {
        std::vector<const Expression*> conditions;
        extract_cnf_conditions(problem_.goal(i).goal_expression(), conditions);

        // Convert expressions to IDs
        for (const Expression* condition : conditions) {
            int goal_id = find_grounded_fluent_id(*condition);
            if (goal_id != -1) {
                goal_condition_ids_.push_back(goal_id);
            }
        }
    }
}

void RelaxedPlanningGraph::extract_cnf_conditions(const Expression& expr, std::vector<const Expression*>& conditions) const {
    if (expr.is_and()) {
        for (size_t i = 0; i < expr.list_size(); ++i) {
            extract_cnf_conditions(expr.list_element(i), conditions);
        }
    } else {
        if (!expr.is_function_symbol()) {
            conditions.push_back(&expr);
        }
    }
}

std::vector<const Action*> RelaxedPlanningGraph::compute_applicable_actions(int layer_index) const {
    std::vector<const Action*> applicable;

    for (size_t i = 0; i < problem_.action_count(); ++i) {
        const Action& action = problem_.action(i);
        bool satisfied = are_preconditions_satisfied(action, layer_index);


        if (satisfied) {
            applicable.push_back(&action);
        }
    }

    return applicable;
}

bool RelaxedPlanningGraph::are_preconditions_satisfied(const Action& action, int layer_index) const {
    if (!action.has_precondition()) {
        return true;
    }

    std::vector<const Expression*> preconditions;
    extract_cnf_conditions(action.precondition(), preconditions);

    for (const Expression* precond : preconditions) {
        if (!is_condition_satisfied(*precond, layer_index)) {
            return false;
        }
    }

    return true;
}

bool RelaxedPlanningGraph::is_condition_satisfied(const Expression& condition, int layer_index) const {
    // Handle negated conditions
    if (is_negated_condition(condition)) {
        const Expression& inner_condition = get_inner_condition(condition);

        if (inner_condition.is_bool_type()) {
            // For negated Boolean: satisfied if positive version is NOT in fact layer
            return !is_positive_condition_satisfied(inner_condition, layer_index);
        } else {
            // For negated numeric: use simplified approach (assume satisfiable)
            return true;
        }
    }

    // Handle positive conditions
    if (condition.is_bool_type()) {
        return is_positive_condition_satisfied(condition, layer_index);
    }

    // For numeric conditions in relaxed planning graph: assume all are satisfiable
    // This is the standard relaxed planning graph approach - ignore resource constraints
    return true;
}

void RelaxedPlanningGraph::add_effects_to_layer(const Action& action, int target_layer_index) {
    for (size_t i = 0; i < action.effect_count(); ++i) {
        const Effect& effect = action.effect(i);
        const Expression& fluent = effect.fluent();

        // In relaxed planning graph, we ignore delete effects
        // Add positive effects and numeric assignments
        if (effect.value().is_atom() && effect.value().value().is_boolean() && effect.value().value().boolean()) {
            int fluent_id = find_grounded_fluent_id(fluent);
            if (fluent_id != -1) {
                // Set automatically handles uniqueness
                fact_layers_[target_layer_index].insert(fluent_id);

                // Track achievability (first occurrence only)
                if (achievability_layer_.find(fluent_id) == achievability_layer_.end()) {
                    achievability_layer_[fluent_id] = target_layer_index;
                }
            }
        } else if (effect.value().is_atom() && !effect.value().value().is_boolean()) {
            // Numeric effect - add the fluent as potentially modified
            int fluent_id = find_grounded_fluent_id(fluent);
            if (fluent_id != -1) {
                fact_layers_[target_layer_index].insert(fluent_id);

                // Track achievability (first occurrence only)
                if (achievability_layer_.find(fluent_id) == achievability_layer_.end()) {
                    achievability_layer_[fluent_id] = target_layer_index;
                }
            }
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


bool RelaxedPlanningGraph::is_numeric_condition_potentially_satisfied(const Expression& condition, int layer_index) const {
    // Simplified approach: if any fluent in the condition has been modified, assume it can be satisfied
    return contains_modified_numeric_fluent(condition);
}

bool RelaxedPlanningGraph::contains_modified_numeric_fluent(const Expression& expr) const {
    std::unordered_set<const Expression*> fluents;
    collect_fluents_in_expression(expr, fluents);

    for (const Expression* fluent : fluents) {
        int fluent_id = find_grounded_fluent_id(*fluent);
        if (fluent_id != -1 && modified_numeric_fluents_.count(fluent_id) > 0) {
            return true;
        }
    }

    return false;
}

void RelaxedPlanningGraph::collect_fluents_in_expression(const Expression& expr, std::unordered_set<const Expression*>& fluents) const {
    if (expr.kind() == Expression::Kind::STATE_VARIABLE) {
        fluents.insert(&expr);
    } else if (expr.is_list()) {
        for (size_t i = 0; i < expr.list_size(); ++i) {
            collect_fluents_in_expression(expr.list_element(i), fluents);
        }
    }
}

void RelaxedPlanningGraph::analyze_numeric_modifications() {
    modified_numeric_fluents_.clear();

    for (size_t i = 0; i < problem_.action_count(); ++i) {
        const Action& action = problem_.action(i);
        for (size_t j = 0; j < action.effect_count(); ++j) {
            const Effect& effect = action.effect(j);
            if (effect.value().is_atom() && !effect.value().value().is_boolean()) {
                // This is a numeric effect
                int fluent_id = find_grounded_fluent_id(effect.fluent());
                if (fluent_id != -1) {
                    modified_numeric_fluents_.insert(fluent_id);
                }
            }
        }
    }
}

int RelaxedPlanningGraph::find_grounded_fluent_id(const Expression& expr) const {
    // Linear search through grounded fluents to find matching expression
    for (size_t i = 0; i < problem_.grounded_fluent_count(); ++i) {
        if (problem_.grounded_fluent(i).to_string() == expr.to_string()) {
            return static_cast<int>(i);
        }
    }
    return -1; // Not found
}

bool RelaxedPlanningGraph::is_negated_condition(const Expression& condition) const {
    return condition.is_not();
}

const Expression& RelaxedPlanningGraph::get_inner_condition(const Expression& negated_condition) const {
    // For (not condition), return the first argument
    return negated_condition.list_element(0);
}

bool RelaxedPlanningGraph::is_positive_condition_satisfied(const Expression& condition, int layer_index) const {
    int fluent_id = find_grounded_fluent_id(condition);
    if (fluent_id == -1) {
        return false; // Unknown fluent
    }

    return fact_layers_[layer_index].count(fluent_id) > 0;
}

} // namespace planmt