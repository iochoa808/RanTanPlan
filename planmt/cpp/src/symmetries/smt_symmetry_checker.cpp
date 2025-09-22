#include "smt_symmetry_checker.hpp"
#include "../problem/visitors/expression_visitor.hpp"
#include "../util/memory_tracker.hpp"
#include "../config/config.hpp"
#include <iostream>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <chrono>

namespace planmt {

SMTSymmetryChecker::SMTSymmetryChecker(const Problem* problem, z3::context& ctx)
    : problem_(problem), context_(ctx), symbol_table_(), visitor_(ctx, symbol_table_, problem) {
    auto& config = Config::instance();
    if (config.is_info()) {
        double current_memory = MemoryTracker::instance().get_current_memory_mb();
        std::cout << "[Symmetry] Starting symmetry detection, memory=" << current_memory << "MB" << std::endl;
    }
}

std::string ActionSwap::to_string() const {
    return action1->name() + "(" + action1->to_string() + ") ↔ " + 
           action2->name() + "(" + action2->to_string() + ")";
}

std::vector<ObjectSwap> SMTSymmetryChecker::detect_all_object_swaps() {
    auto& config = Config::instance();
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<ObjectSwap> detected_swaps;
    
    // Clear previous results
    detected_symmetries_.clear();
    
    // Group objects by type - only objects of same type can be symmetric
    auto objects_by_type = get_objects_by_type();
    
    
    // For each type with multiple objects, check for symmetries using SMT
    for (const auto& [type_name, objects] : objects_by_type) {
        if (objects.size() < 2) {
            continue; // Need at least 2 objects to have symmetry
        }
        
        
        // Check all pairs of objects of this type using SMT
        for (size_t i = 0; i < objects.size(); i++) {
            for (size_t j = i + 1; j < objects.size(); j++) {
                const std::string& obj1_name = objects[i]->name();
                const std::string& obj2_name = objects[j]->name();
                
                if (are_objects_symmetric(obj1_name, obj2_name)) {
                    ObjectSwap swap{obj1_name, obj2_name, type_name};
                    detected_swaps.push_back(swap);
                    
                    // Get and store the variable pairs and action pairs
                    auto variable_pairs = get_symmetric_variable_pairs(obj1_name, obj2_name);
                    auto action_pairs = get_symmetric_action_pairs(obj1_name, obj2_name);
                    detected_symmetries_.emplace_back(swap, variable_pairs, action_pairs);
                    
                } 
            }
        }
    }
    
    if (config.is_debug()) {
        std::cout << "Detected " << detected_symmetries_.size() << " symmetric object pairs:" << std::endl;
        for (const auto& symmetry : detected_symmetries_) {
            std::cout << "  " << symmetry.object_swap.to_string() << " -> " << symmetry.variable_pairs.size() 
                      << " variable pairs, " << symmetry.action_pairs.size() << " action pairs" << std::endl;
        }
    }
    
    // Print timing and memory info for completion
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration<double>(end_time - start_time).count();
    if (config.is_info()) {
        double current_memory = MemoryTracker::instance().get_current_memory_mb();
        std::cout << "[Symmetry] detection took: time=" << total_time << "s, memory=" << current_memory << "MB";
        std::cout << std::endl;
    }
    
    return detected_swaps;
}

bool SMTSymmetryChecker::are_objects_symmetric(const std::string& obj1, const std::string& obj2) {
    // Clear visitor state and symbol table for clean start
    visitor_.clear();
    symbol_table_.clear();
    
    // Create Z3 integer constants for the two objects being swapped
    z3::expr obj1_const = context_.int_const(obj1.c_str());
    z3::expr obj2_const = context_.int_const(obj2.c_str());
    
    // Build original problem formula using temporal encoding
    z3::expr_vector original_constraints(context_);
    
    // Add initial state constraints at timestep 0
    visitor_.set_timestep(0);
    for (const auto& assignment : problem_->initial_state()) {
        auto fluent_expr = convert_expression_to_z3(assignment.fluent());
        auto value_expr = convert_expression_to_z3(assignment.value());
        
        if (fluent_expr && value_expr) {
            z3::expr constraint = (*fluent_expr) == (*value_expr);
            original_constraints.push_back(constraint);
        }
    }
    
    // Add goal constraints at timestep 1 (different from initial state)
    visitor_.set_timestep(1);
    for (const auto& goal : problem_->goals()) {
        auto goal_expr = convert_expression_to_z3(goal.goal_expression());
        if (goal_expr) {
            original_constraints.push_back(*goal_expr);
        }
    }
    
    // Clear timestep after use
    visitor_.clear_timestep();
    
    // Create swapped problem by substituting obj1 ↔ obj2 in all constraints
    z3::expr_vector swapped_constraints(context_);
    z3::expr_vector from_vec(context_);
    z3::expr_vector to_vec(context_);
    from_vec.push_back(obj1_const);
    from_vec.push_back(obj2_const);
    to_vec.push_back(obj2_const);
    to_vec.push_back(obj1_const);
    
    for (const auto& constraint : original_constraints) {
        z3::expr constraint_copy = constraint;
        z3::expr swapped = constraint_copy.substitute(from_vec, to_vec);
        swapped_constraints.push_back(swapped);
    }
    
    // Check semantic equivalence using logical biconditional
    z3::expr original_formula = original_constraints.empty() ? 
        context_.bool_val(true) : z3::mk_and(original_constraints);
    z3::expr swapped_formula = swapped_constraints.empty() ? 
        context_.bool_val(true) : z3::mk_and(swapped_constraints);
    
    // Objects are symmetric if: ¬(original ↔ swapped) is UNSAT
    z3::solver solver(context_);
    z3::expr equivalence = (original_formula == swapped_formula);
    solver.add(!equivalence);
    z3::check_result result = solver.check();
    
    return result == z3::unsat;
}

std::unordered_map<std::string, std::vector<const Object*>> 
SMTSymmetryChecker::get_objects_by_type() const {
    std::unordered_map<std::string, std::vector<const Object*>> result;
    
    for (size_t i = 0; i < problem_->object_count(); i++) {
        const Object& obj = problem_->object(i);
        const std::string type_name = obj.type() ? obj.type()->name() : "unknown";
        result[type_name].push_back(&obj);
    }
    return result;
}


std::optional<z3::expr> SMTSymmetryChecker::convert_expression_to_z3(const Expression& expr) {
    visitor_.clear();
    accept_visitor(expr, visitor_);
    return visitor_.get_result();
}

std::vector<std::pair<const Expression*, const Expression*>> SMTSymmetryChecker::get_symmetric_variable_pairs(
    const std::string& obj1, const std::string& obj2) const {
    std::vector<std::pair<const Expression*, const Expression*>> variable_pairs;
    
    // Collect all fluents from initial state that involve either symmetric object
    std::vector<const Expression*> relevant_fluents;
    for (const auto& assignment : problem_->initial_state()) {
        const Expression& fluent = assignment.fluent();
        if (expression_involves_object(fluent, obj1) || expression_involves_object(fluent, obj2)) {
            relevant_fluents.push_back(&fluent);
        }
    }
    
    
    // For each pair of relevant fluents, check if they are symmetric with respect to obj1 and obj2
    for (size_t i = 0; i < relevant_fluents.size(); i++) {
        for (size_t j = i + 1; j < relevant_fluents.size(); j++) {
            const Expression* fluent1 = relevant_fluents[i];
            const Expression* fluent2 = relevant_fluents[j];
            
            // Check if these two fluents are symmetric with respect to obj1 and obj2
            if (are_expressions_symmetric(*fluent1, *fluent2, obj1, obj2)) {
                variable_pairs.push_back({fluent1, fluent2});
            }
        }
    }
    
    return variable_pairs;
}

bool SMTSymmetryChecker::expression_involves_object(const Expression& expr, const std::string& obj_name) const {
    // Check if this is a constant atom matching the object name
    if (expr.is_constant() && expr.is_atom() && expr.value().symbol() == obj_name) {
        return true;
    }
    
    // Check all sub-expressions in the list
    for (size_t i = 0; i < expr.list_size(); i++) {
        const Expression& param = expr.list_element(i);
        if (expression_involves_object(param, obj_name)) {
            return true;
        }
    }
    return false;
}

bool SMTSymmetryChecker::are_expressions_symmetric(const Expression& expr1, const Expression& expr2, 
                                                   const std::string& obj1, const std::string& obj2) const {
    // Base case: both are atoms
    if (expr1.is_atom() && expr2.is_atom()) {
        if (expr1.is_constant() && expr2.is_constant()) {
            std::string name1 = expr1.value().symbol();
            std::string name2 = expr2.value().symbol();
            
            // They are symmetric if one has obj1 where the other has obj2, or vice versa
            if (name1 == obj1 && name2 == obj2) return true;
            if (name1 == obj2 && name2 == obj1) return true;
            // Or if they're identical but don't involve the symmetric objects
            if (name1 == name2 && name1 != obj1 && name1 != obj2) return true;
            return false;
        }
        // For non-constant atoms, they should be identical
        return expr1 == expr2;
    }
    
    // Both should be lists
    if (!expr1.is_list() || !expr2.is_list()) {
        return false;
    }
    
    // Lists should have the same size
    if (expr1.list_size() != expr2.list_size()) {
        return false;
    }
    
    // Recursively check all elements
    for (size_t i = 0; i < expr1.list_size(); i++) {
        if (!are_expressions_symmetric(expr1.list_element(i), expr2.list_element(i), obj1, obj2)) {
            return false;
        }
    }
    
    return true;
}

std::vector<ActionSwap> SMTSymmetryChecker::get_symmetric_action_pairs(
    const std::string& obj1, const std::string& obj2) const {
    std::vector<ActionSwap> action_pairs;
    
    // Get all actions from the problem
    const auto& actions = problem_->actions();
    
    // Check all pairs of actions for symmetry
    for (size_t i = 0; i < actions.size(); i++) {
        for (size_t j = i + 1; j < actions.size(); j++) {
            const Action& action1 = actions[i];
            const Action& action2 = actions[j];
            
            // Check if these actions are symmetric with respect to obj1 and obj2
            if (are_actions_symmetric_by_signature(action1, action2, obj1, obj2)) {
                action_pairs.push_back(ActionSwap{&action1, &action2});
            }
        }
    }
    
    return action_pairs;
}

bool SMTSymmetryChecker::are_actions_symmetric_by_signature(const Action& action1, const Action& action2, 
                                                           const std::string& obj1, const std::string& obj2) const {
    
    // Extract parameters from grounded action name
    // E.g. "board_person2_plane1_city0" -> ["person2", "plane1", "city0"]
    auto extract_parameters = [](const std::string& grounded_name) -> std::vector<std::string> {
        std::vector<std::string> params;
        size_t start = grounded_name.find('_');
        if (start == std::string::npos) {
            return params; // No parameters
        }
        start++; // Skip the first underscore
        
        size_t pos = start;
        while (pos < grounded_name.length()) {
            size_t next_underscore = grounded_name.find('_', pos);
            if (next_underscore == std::string::npos) {
                // Last parameter
                params.push_back(grounded_name.substr(pos));
                break;
            } else {
                params.push_back(grounded_name.substr(pos, next_underscore - pos));
                pos = next_underscore + 1;
            }
        }
        return params;
    };
    
    // Extract base action name (template name) from grounded action name
    // E.g. "board_person2_plane1_city0" -> "board"
    auto extract_base_name = [](const std::string& grounded_name) -> std::string {
        size_t first_underscore = grounded_name.find('_');
        if (first_underscore != std::string::npos) {
            return grounded_name.substr(0, first_underscore);
        }
        return grounded_name; // No underscore found, return as-is
    };
    
    std::string base_name1 = extract_base_name(action1.name());
    std::string base_name2 = extract_base_name(action2.name());
    
    // Actions must have the same base action name
    if (base_name1 != base_name2) return false;
    
    // Extract parameters from action names
    std::vector<std::string> params1 = extract_parameters(action1.name());
    std::vector<std::string> params2 = extract_parameters(action2.name());
    
    // Actions must have the same number of parameters
    if (params1.size() != params2.size()) return false;
    
    // Check if parameters differ only by obj1<->obj2 swap
    bool has_symmetric_swap = false;
    for (size_t i = 0; i < params1.size(); i++) {
        const std::string& param1 = params1[i];
        const std::string& param2 = params2[i];
        
        if (param1 == param2) {
            // Same parameter object, acceptable 
            continue;
        } else if ((param1 == obj1 && param2 == obj2) || (param1 == obj2 && param2 == obj1)) {
            // This parameter position has the symmetric objects swapped
            has_symmetric_swap = true;
            continue;
        } else {
            // Parameters differ in a way that's not related to the obj1<->obj2 swap
            return false;
        }
    }
    
    // For actions to be symmetric, they must have at least one parameter position where obj1 and obj2 are swapped
    if (!has_symmetric_swap) {
        return false;
    }
    return true;
}

// ========================================================================
// PUBLIC INTERFACE IMPLEMENTATIONS
// ========================================================================

std::vector<ObjectSwap> SMTSymmetryChecker::get_object_swaps() const {
    std::vector<ObjectSwap> swaps;
    for (const auto& symmetry : detected_symmetries_) {
        swaps.push_back(symmetry.object_swap);
    }
    return swaps;
}

std::vector<std::pair<const Expression*, const Expression*>> 
SMTSymmetryChecker::get_variable_pairs_for_swap(const std::string& obj1, const std::string& obj2) const {
    for (const auto& symmetry : detected_symmetries_) {
        const ObjectSwap& swap = symmetry.object_swap;
        if ((swap.obj1_name == obj1 && swap.obj2_name == obj2) ||
            (swap.obj1_name == obj2 && swap.obj2_name == obj1)) {
            return symmetry.variable_pairs;
        }
    }
    return {}; // Empty vector if no symmetry found
}

bool SMTSymmetryChecker::are_objects_known_symmetric(const std::string& obj1, const std::string& obj2) const {
    for (const auto& symmetry : detected_symmetries_) {
        const ObjectSwap& swap = symmetry.object_swap;
        if ((swap.obj1_name == obj1 && swap.obj2_name == obj2) ||
            (swap.obj1_name == obj2 && swap.obj2_name == obj1)) {
            return true;
        }
    }
    return false;
}

std::vector<ActionSwap> SMTSymmetryChecker::get_action_pairs_for_swap(const std::string& obj1, const std::string& obj2) const {
    for (const auto& symmetry : detected_symmetries_) {
        const ObjectSwap& swap = symmetry.object_swap;
        if ((swap.obj1_name == obj1 && swap.obj2_name == obj2) ||
            (swap.obj1_name == obj2 && swap.obj2_name == obj1)) {
            return symmetry.action_pairs;
        }
    }
    return {}; // Empty vector if no symmetry found
}

} // namespace planmt