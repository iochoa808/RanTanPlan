#include "relaxed_planning_graph.h"
#include "../config/config.h"
#include "../util/memory_tracker.h"
#include <iostream>
#include <iomanip>

namespace planmt {

// Static member definitions
const std::vector<const Action*> RelaxedPlanningGraph::empty_action_vector_;
const std::unordered_set<int> RelaxedPlanningGraph::empty_condition_set_;

RelaxedPlanningGraph::RelaxedPlanningGraph(const Problem& problem)
    : problem_(problem), early_termination_on_goals_(true), build_time_ms_(0.0) {
    extract_goal_conditions();
    analyze_numeric_modifications();
}

bool RelaxedPlanningGraph::build() {
    auto& config = Config::instance();
    build_start_time_ = std::chrono::high_resolution_clock::now();
    double start_memory = MemoryTracker::instance().get_current_memory_mb();

    reset();
    initialize_fact_layer();

    // Check if goals are already achievable in the initial state (if early termination is enabled)
    if (early_termination_on_goals_ && are_goals_achievable()) {
        auto end_time = std::chrono::high_resolution_clock::now();
        build_time_ms_ = std::chrono::duration<double, std::milli>(end_time - build_start_time_).count();

        // Compact output
        std::cout << "RPG built in " << static_cast<int>(build_time_ms_) << "ms" << std::endl;
        std::cout << "Goals reachable: YES" << std::endl;
        std::cout << "Total layers: " << fact_layers_.size() << std::endl;

        if (config.is_debug()) {
            print_debug_info();
            print_reachability_analysis();
        }

        return true;
    }

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

        // Create next fact layer - copy all facts from current layer (no delete effects in RPG)
        fact_layers_.emplace_back(fact_layers_[current_layer]);
        int next_layer = fact_layers_.size() - 1;

        // Add effects of applicable actions
        for (const Action* action : action_layers_[current_layer]) {
            add_effects_to_layer(*action, next_layer);
        }

        // Check if goals are now achievable (if early termination is enabled)
        if (early_termination_on_goals_ && are_goals_achievable()) {
            break;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    build_time_ms_ = std::chrono::duration<double, std::milli>(end_time - build_start_time_).count();
    double end_memory = MemoryTracker::instance().get_current_memory_mb();

    bool goals_reachable = are_goals_achievable();

    // Compact output similar to ARPG
    std::cout << "RPG built in " << static_cast<int>(build_time_ms_) << "ms" << std::endl;
    std::cout << "Goals reachable: " << (goals_reachable ? "YES" : "NO") << std::endl;
    std::cout << "Total layers: " << fact_layers_.size() << std::endl;

    if (config.is_info()) {
        std::cout << "[RPG] construction took: time=" << (build_time_ms_ / 1000.0) << "s"
                  << ", memory=" << end_memory << "MB"
                  << ", layers=" << fact_layers_.size() << std::endl;
    }

    if (config.is_debug()) {
        print_debug_info();
        print_reachability_analysis();
    }

    return goals_reachable;
}

bool RelaxedPlanningGraph::is_achievable(const Expression& condition) const {
    int fluent_id = find_grounded_fluent_id(condition);
    return fluent_id != -1 && achievability_layer_.find(fluent_id) != achievability_layer_.end();
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
    return (layer >= 0 && layer < static_cast<int>(action_layers_.size())) ? action_layers_[layer] : empty_action_vector_;
}

const std::unordered_set<int>& RelaxedPlanningGraph::get_conditions_in_layer(int layer) const {
    return (layer >= 0 && layer < static_cast<int>(fact_layers_.size())) ? fact_layers_[layer] : empty_condition_set_;
}

bool RelaxedPlanningGraph::are_goals_achievable() const {
    // If no Boolean goal conditions were extracted (e.g., only numeric goals),
    // we cannot reliably determine achievability in the RPG context without
    // proper numeric analysis. For reachability analysis, we should continue
    // building the RPG to see what actions become applicable.
    if (goal_condition_ids_.empty()) {
        return true;
    }

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

    std::cout << "Modified numeric fluents: " << modified_numeric_fluents_.size() << "\n";
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

    // List unreachable fluents (up to 10 for brevity)
    if (unreachable_fluents > 0) {
        std::cout << "\nUnreachable fluents (showing up to 10):\n";
        int count = 0;
        for (size_t i = 0; i < total_grounded_fluents && count < 10; ++i) {
            if (all_reached_fluents.find(static_cast<int>(i)) == all_reached_fluents.end()) {
                std::cout << "  " << problem_.grounded_fluent(i).to_string() << "\n";
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

void RelaxedPlanningGraph::initialize_fact_layer() {
    fact_layers_.emplace_back();

    // Track which positive Boolean fluents are true in initial state
    std::unordered_set<int> initially_true_fluents;

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
                initially_true_fluents.insert(fluent_id);
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

    // For all Boolean fluents not initially true, add their negations as facts
    for (size_t i = 0; i < problem_.grounded_fluent_count(); ++i) {
        const Expression& fluent = problem_.grounded_fluent(i);

        // Only consider Boolean fluents
        if (fluent.is_bool_type()) {
            int fluent_id = static_cast<int>(i);

            // If this fluent is not initially true, add its negation
            if (initially_true_fluents.count(fluent_id) == 0) {
                int negative_fluent_id = encode_negative_fact_id(fluent_id);
                fact_layers_[0].insert(negative_fluent_id);
                achievability_layer_[negative_fluent_id] = 0;
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
            // Note: Numeric conditions (like >= comparisons) are not stored as goal IDs
            // since they can't be mapped to grounded fluents. For RPG purposes,
            // we assume all numeric conditions are satisfiable once modified fluents exist.
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

        // Debug: Print action applicability for layer 0
        // if (layer_index == 0) {
        //     std::cout << "Action " << action.name() << ": " << (satisfied ? "APPLICABLE" : "NOT APPLICABLE") << std::endl;
        // }

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
        bool satisfied = is_condition_satisfied(*precond, layer_index);

        // Debug: Print precondition satisfaction for layer 0
        // if (layer_index == 0) {
        //     std::cout << "  Precondition '" << precond->to_string() << "': " << (satisfied ? "SATISFIED" : "NOT SATISFIED") << std::endl;
        // }

        if (!satisfied) {
            return false;
        }
    }

    return true;
}

bool RelaxedPlanningGraph::is_condition_satisfied(const Expression& condition, int layer_index) const {
    // Both positive and negative Boolean conditions are handled the same way:
    // check if the condition (or its negation) exists as a fact in the layer
    if (condition.is_bool_type() || is_negated_condition(condition)) {
        bool result = is_fact_in_layer(condition, layer_index);

        // Debug: Print detailed fact checking for layer 0
        // if (layer_index == 0) {
        //     int fluent_id = find_grounded_fluent_id(condition);
        //     std::cout << "    Checking condition '" << condition.to_string() << "' -> fluent_id=" << fluent_id;
        //     if (fluent_id != -1) {
        //         bool in_layer = fact_layers_[layer_index].count(fluent_id) > 0;
        //         std::cout << ", in_layer=" << (in_layer ? "true" : "false");
        //     }
        //     std::cout << ", result=" << (result ? "true" : "false") << std::endl;
        // }

        return result;
    }

    // For numeric conditions in relaxed planning graph: assume all are satisfiable
    // This is the standard relaxed planning graph approach - ignore resource constraints
    return true;
}

void RelaxedPlanningGraph::add_effects_to_layer(const Action& action, int target_layer_index) {
    for (size_t i = 0; i < action.effect_count(); ++i) {
        const Effect& effect = action.effect(i);

        // For conditional and quantified effects in RPG: be optimistic and assume they can be applied
        // This is the standard relaxed planning graph approach
        if (effect.is_conditional() || effect.is_quantified()) {
            add_conditional_or_quantified_effect_to_layer(effect, target_layer_index);
        } else {
            // Handle simple atomic effects as before
            add_simple_effect_to_layer(effect, target_layer_index);
        }
    }
}

void RelaxedPlanningGraph::add_simple_effect_to_layer(const Effect& effect, int target_layer_index) {
    const Expression& fluent = effect.fluent();

    // Handle both positive and negative Boolean effects
    if (effect.value().is_atom() && effect.value().value().is_boolean()) {
        int fluent_id = find_grounded_fluent_id(fluent);
        if (fluent_id != -1) {
            if (effect.value().value().boolean()) {
                // Positive effect: add the positive fact
                fact_layers_[target_layer_index].insert(fluent_id);
                if (achievability_layer_.find(fluent_id) == achievability_layer_.end()) {
                    achievability_layer_[fluent_id] = target_layer_index;
                }
            } else {
                // Negative effect: add the negative fact
                int negative_fluent_id = encode_negative_fact_id(fluent_id);
                fact_layers_[target_layer_index].insert(negative_fluent_id);
                if (achievability_layer_.find(negative_fluent_id) == achievability_layer_.end()) {
                    achievability_layer_[negative_fluent_id] = target_layer_index;
                }
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

void RelaxedPlanningGraph::add_conditional_or_quantified_effect_to_layer(const Effect& effect, int target_layer_index) {
    // For conditional and quantified effects in relaxed planning graph: be optimistic
    // Assume the effect CAN be applied (ignoring conditions and quantifiers)
    // This follows the standard relaxed planning graph semantics

    // Extract the underlying atomic effect and add it optimistically
    add_simple_effect_to_layer(effect, target_layer_index);

    // TODO: For full completeness, we could analyze the condition/quantifier to extract
    // additional facts that might become true, but for basic RPG this optimistic approach suffices
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
    // Handle negated expressions
    if (is_negated_condition(expr)) {
        const Expression& inner_condition = get_inner_condition(expr);
        // Use O(1) hash map lookup for the positive fluent ID and return the negative encoding
        int fluent_id = problem_.find_grounded_fluent_index(inner_condition);
        return (fluent_id != -1) ? encode_negative_fact_id(fluent_id) : -1;
    }

    // Handle positive expressions - use O(1) hash map lookup
    return problem_.find_grounded_fluent_index(expr);
}

bool RelaxedPlanningGraph::is_negated_condition(const Expression& condition) const {
    return condition.is_not();
}

const Expression& RelaxedPlanningGraph::get_inner_condition(const Expression& negated_condition) const {
    // For (not condition), return the condition being negated
    // The structure is: (not condition) where list_element(0) = 'not', list_element(1) = condition
    if (negated_condition.list_size() >= 2) {
        return negated_condition.list_element(1);
    } else {
        // Fallback to original behavior if structure is unexpected
        return negated_condition.list_element(0);
    }
}

bool RelaxedPlanningGraph::is_fact_in_layer(const Expression& condition, int layer_index) const {
    int fluent_id = find_grounded_fluent_id(condition);
    return fluent_id != -1 && fact_layers_[layer_index].count(fluent_id) > 0;
}

std::string RelaxedPlanningGraph::fact_id_to_string(int fact_id) const {
    if (fact_id >= 0) {
        // Positive fact
        if (fact_id < static_cast<int>(problem_.grounded_fluent_count())) {
            return problem_.grounded_fluent(fact_id).to_string();
        }
    } else {
        // Negative fact: decode to get original fluent_id
        int original_fluent_id = decode_negative_fact_id(fact_id);
        if (original_fluent_id >= 0 && original_fluent_id < static_cast<int>(problem_.grounded_fluent_count())) {
            return "(not " + problem_.grounded_fluent(original_fluent_id).to_string() + ")";
        }
    }
    return "unknown_fact_" + std::to_string(fact_id);
}

} // namespace planmt