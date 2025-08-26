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
                    if (Config::instance().is_debug()) {
                        std::cout << "Detected symmetry: " << swap.to_string() << std::endl;
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

} // namespace planmt