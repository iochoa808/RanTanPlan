#include "reified_grounded_encoder.h"
#include "parallelism/parallelism_factory.h"
#include <iostream>
#include <cassert>
#include "problem/visitors/print_visitor.h"
#include "problem/visitors/expression_visitor.h"
#include "../config/config.h"
#include <functional>

namespace planmt {

// Constructor
ReifiedGroundedEncoder::ReifiedGroundedEncoder(const Problem& problem, z3::context& ctx)
    : GroundedEncoder(problem, ctx), reified_variable_factory_(ctx) {
}


std::shared_ptr<z3::expr> ReifiedGroundedEncoder::encode_actions(int t) {
    z3::expr_vector action_constraints(ctx_);

    for (const Action& action : problem_.actions()) {
        z3::expr action_var = variable_factory_.get_action_variable(action, t); 
        
        // if the action has no effects, we skip it as it cannot change any fluents
        if (!action.effects().empty()) {

            // Create reified precondition constraints for CNF preconditions
            if (action.has_precondition()) {
                const Expression& precond = action.precondition();
                std::string action_name = variable_factory_.get_action_var_name(action);
                std::string prefix = action_name + "_precond";
                
                // Create reified constraints for each clause in the CNF
                std::vector<z3::expr> reified_constraints = create_reified_cnf_constraints(precond, t, prefix);
                
                // Add the reified constraints (r_i <-> clause_i)
                for (const auto& reified_constraint : reified_constraints) {
                    action_constraints.push_back(reified_constraint);
                }
                
                // Action implies all reified clauses are true: action_var -> r_1 /\ action_var -> r_2 /\ ...
                auto reified_vars_with_names = reified_variable_factory_.get_all_reified_clause_variables_with_names(t);
                for (const auto& [reified_var, var_name] : reified_vars_with_names) {
                    if (var_name.find(prefix) == 0) {
                        z3::expr implication = z3::implies(action_var, reified_var);
                        action_constraints.push_back(implication);
                    }
                }
            }
        
            // Create effect constraints: action_var => effects
            z3::expr_vector effect_exprs(ctx_);
            for (const Effect& effect : action.effects()) {
                auto z3_effect = convert_effect_to_z3(effect.effect_expression(), t);
                if (z3_effect.has_value()) {
                    effect_exprs.push_back(z3_effect.value());
                }
            }
            z3::expr effect_conjunction = z3::mk_and(effect_exprs);
            // action_var => effect_conjunction
            //std::cout << "eff:" << action_var.to_string() << " -> " << effect_conjunction.to_string() << std::endl;
            action_constraints.push_back(z3::implies(action_var, effect_conjunction));
        }
    }
    
    // Combine all action constraints. Can't see why we could have no actions, but handle it gracefully.
    if (action_constraints.empty()) {
        auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
        return expr;
    }
    return std::make_shared<z3::expr>(z3::mk_and(action_constraints));
}


std::shared_ptr<z3::expr> ReifiedGroundedEncoder::encode_goal(int t) {
    // Retrieve goals from the problem
    const auto& goals = problem_.goals();
    
    if (goals.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true)); // If no goals, vacuously satisfied
    }
    
    // Collect all goal constraints (both reified and regular)
    std::vector<z3::expr> goal_constraints;
    
    int goal_idx = 0;
    for (const auto& goal : goals) {
        const Expression& goal_expr = goal.goal_expression();
        std::string prefix = "goal" + std::to_string(goal_idx);
        
        // Create reified constraints for each clause in the CNF goal
        std::vector<z3::expr> reified_constraints = create_reified_cnf_constraints(goal_expr, t, prefix);
        
        // Add the reified constraints (r_i <-> clause_i)
        for (const auto& reified_constraint : reified_constraints) {
            goal_constraints.push_back(reified_constraint);
        }
        
        // All reified clauses for this goal must be true
        auto reified_vars_with_names = reified_variable_factory_.get_all_reified_clause_variables_with_names(t);
        for (const auto& [reified_var, var_name] : reified_vars_with_names) {
            if (var_name.find(prefix) == 0) {
                goal_constraints.push_back(reified_var);
            }
        }
        
        goal_idx++;
    }
    
    // Combine all goal constraints with logical AND
    if (goal_constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    
    // Create a flat conjunction using Z3's mk_and function
    z3::expr_vector goal_vector(ctx_);
    for (const auto& constraint : goal_constraints) {
        goal_vector.push_back(constraint);
    }
    z3::expr goal_conjunction = z3::mk_and(goal_vector);
    return std::make_shared<z3::expr>(goal_conjunction);
}

bool ReifiedGroundedEncoder::looks_like_a_cnf(const Expression& expr) const {
    // CNF is an AND of clauses (ORs) or literals
    if (!expr.is_and()) {
        return false;
    }
    
    // Check that all children are either single elements or OR clauses
    const auto& list = expr.list();
    for (size_t i = 1; i < list.size(); ++i) {  // Skip the AND operator at index 0
        const auto& child = list[i];
        // Each child should be either a single element (atom/literal) or an OR clause
        // Include function applications (like numeric comparisons) as valid clauses
        if (!child.is_atom() && !child.is_or() && !child.is_not() && 
            !child.is_state_variable() && !child.is_constant() && !child.is_function_application()) {
            return false;
        }
    }
    
    return true;
}

std::vector<z3::expr> ReifiedGroundedEncoder::create_reified_cnf_constraints(const Expression& cnf_expr, int timestep, const std::string& prefix) {
    std::vector<z3::expr> constraints;
    
    if (looks_like_a_cnf(cnf_expr)) {
        // Handle AND of clauses/literals - each element gets reified
        // Skip the first element (the AND operator) and process only the operands
        int clause_idx = 0;
        const auto& list = cnf_expr.list();
        for (size_t i = 1; i < list.size(); ++i) {  // Start from index 1 to skip the AND operator
            const auto& clause = list[i];
            
            
            std::string clause_name = prefix + "_c" + std::to_string(clause_idx);
            z3::expr reified_constraint = create_reified_clause_constraint(clause, timestep, clause_name);
            constraints.push_back(reified_constraint);
            clause_idx++;
        }
    } else {
        // Single clause/literal case - still needs reification
        std::string clause_name = prefix + "_c0";
        z3::expr reified_constraint = create_reified_clause_constraint(cnf_expr, timestep, clause_name);
        constraints.push_back(reified_constraint);
    }
    
    return constraints;
}

z3::expr ReifiedGroundedEncoder::create_reified_clause_constraint(const Expression& clause_expr, int timestep, const std::string& clause_name) {
    // Create reified variable for this clause using the specialized factory
    z3::expr reified_var = reified_variable_factory_.get_reified_clause_variable(clause_name, timestep);
    
    // Convert clause to Z3 expression
    auto z3_clause = convert_expression_to_z3(clause_expr, timestep);
    
    // Must succeed - fail loudly if conversion fails
    assert(z3_clause.has_value() && "Failed to convert clause expression to Z3");
    
    // Create equivalence: reified_var <-> clause
    z3::expr constraint = (reified_var == *z3_clause);
    
    return constraint;
}


// ============================================================================
// ReifiedZ3VariableFactory implementation
// ============================================================================

z3::expr ReifiedZ3VariableFactory::get_reified_clause_variable(const std::string& clause_name, int timestep) {
    ensure_reified_timestep_capacity(timestep);
    
    // Create timestep-indexed variable name (following the same pattern as action/fluent variables)
    std::string timestep_clause_name = clause_name + "_" + std::to_string(timestep);
    
    auto& timestep_vars = reified_clause_vars_[timestep];
    auto it = timestep_vars.find(clause_name);
    if (it == timestep_vars.end()) {
        // Create new reified clause variable (always boolean)
        z3::expr new_var = create_bool_variable(timestep_clause_name);
        timestep_vars[clause_name] = std::make_shared<z3::expr>(new_var);
        return new_var;
    }
    return *(it->second);
}

std::vector<z3::expr> ReifiedZ3VariableFactory::get_all_reified_clause_variables(int timestep) const {
    std::vector<z3::expr> vars;
    if (timestep >= 0 && timestep < static_cast<int>(reified_clause_vars_.size())) {
        const auto& timestep_vars = reified_clause_vars_[timestep];
        vars.reserve(timestep_vars.size());
        for (const auto& [name, var] : timestep_vars) {
            vars.push_back(*var);
        }
    }
    return vars;
}

std::vector<std::pair<z3::expr, std::string>> ReifiedZ3VariableFactory::get_all_reified_clause_variables_with_names(int timestep) const {
    std::vector<std::pair<z3::expr, std::string>> vars;
    if (timestep >= 0 && timestep < static_cast<int>(reified_clause_vars_.size())) {
        const auto& timestep_vars = reified_clause_vars_[timestep];
        vars.reserve(timestep_vars.size());
        for (const auto& [name, var] : timestep_vars) {
            vars.emplace_back(*var, name);
        }
    }
    return vars;
}

void ReifiedZ3VariableFactory::ensure_reified_timestep_capacity(int timestep) {
    while (static_cast<int>(reified_clause_vars_.size()) <= timestep) {
        reified_clause_vars_.emplace_back();
    }
}


} // namespace planmt
