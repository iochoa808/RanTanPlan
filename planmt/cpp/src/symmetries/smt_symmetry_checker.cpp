#include "smt_symmetry_checker.h"
#include "../problem/visitors/expression_visitor.h"
#include "../util/memory_tracker.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <unordered_set>

namespace planmt {

std::vector<ObjectSwap> SMTSymmetryChecker::detect_all_object_swaps() {
    std::vector<ObjectSwap> detected_swaps;
    
    // Clear previous results
    detected_symmetries_.clear();
    
    // Track memory usage before starting
    double initial_memory = MemoryTracker::instance().get_current_memory_mb();
    
    // Group objects by type - only objects of same type can be symmetric
    auto objects_by_type = get_objects_by_type();
    
    if (Config::instance().is_info()) {
        std::cout << "Symmetry Detection: " << problem_->object_count() << " objects" << std::endl;
        std::cout << "Memory usage: " << initial_memory << " MB" << std::endl;
    }
    
    // For each type with multiple objects, check for symmetries using SMT
    for (const auto& [type_name, objects] : objects_by_type) {
        if (objects.size() < 2) {
            continue; // Need at least 2 objects to have symmetry
        }
        
        if (Config::instance().is_verbose()) {
            std::cout << "\nChecking type '" << type_name << "' with " << objects.size() << " objects" << std::endl;
        }
        
        // Check all pairs of objects of this type using SMT
        for (size_t i = 0; i < objects.size(); i++) {
            for (size_t j = i + 1; j < objects.size(); j++) {
                const std::string& obj1_name = objects[i]->name();
                const std::string& obj2_name = objects[j]->name();
                
                if (are_objects_symmetric(obj1_name, obj2_name)) {
                    ObjectSwap swap{obj1_name, obj2_name, type_name};
                    detected_swaps.push_back(swap);
                    
                    // Get and store the variable pairs
                    auto variable_pairs = get_symmetric_variable_pairs(obj1_name, obj2_name);
                    detected_symmetries_.emplace_back(swap, variable_pairs);
                    
                    if (Config::instance().is_debug()) {
                        std::cout << "Detected symmetry: " << swap.to_string() << std::endl;
                        std::cout << "  Related state variable pairs (" << variable_pairs.size() << " found):" << std::endl;
                        for (const auto& [var1_ptr, var2_ptr] : variable_pairs) {
                            std::cout << "    (" << var1_ptr->to_string() << ", " << var2_ptr->to_string() << ")" << std::endl;
                        }
                    }
                } 
            }
        }
    }
    
    // Track memory usage after completion
    double final_memory = MemoryTracker::instance().get_current_memory_mb();
    
    if (Config::instance().is_info()) {
        std::cout << "Detected " << detected_swaps.size() << " symmetric object pairs using SMT verification" << std::endl;
        std::cout << "Memory usage: " << final_memory << " MB" << std::endl;
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
    
    if (result == z3::unsat && Config::instance().is_debug()) {
        std::cout << "    Symmetric: " << obj1 << " <-> " << obj2 << std::endl;
    }
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
    
    if (Config::instance().is_debug()) {
        std::cout << "    Found " << relevant_fluents.size() << " fluents involving " << obj1 << " or " << obj2 << std::endl;
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

} // namespace planmt