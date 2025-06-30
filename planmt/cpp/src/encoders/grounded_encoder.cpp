#include "grounded_encoder.h"
#include <iostream>
#include "problem/visitors/print_visitor.h"
#include "problem/visitors/expression_visitor.h"
#include <functional>

namespace planmt {

// Constructor
GroundedEncoder::GroundedEncoder(const Problem& problem, z3::context& ctx)
    : problem_(problem), ctx_(ctx), variable_factory_(ctx), grounded_visitor_(ctx_, &problem_, &variable_factory_) {
    layers_encoded_ = -1;
    build_epc_index();
}

// Helper function to convert expression to Z3 using visitor
std::optional<z3::expr> GroundedEncoder::convert_expression_to_z3(const Expression& expr, int timestep) {
    grounded_visitor_.clear(); // start with a fresh visitor state

    if (timestep >= 0) {
        grounded_visitor_.set_timestep(timestep); // Set timestep if provided
    } else {
        grounded_visitor_.clear_timestep();
    }
    accept_visitor(expr, grounded_visitor_);
    grounded_visitor_.clear_timestep(); // Clear timestep after use
    return grounded_visitor_.get_result();
}

// Helper function to convert effect to Z3 constraint using visitor
std::optional<z3::expr> GroundedEncoder::convert_effect_to_z3(const EffectExpression& effect, int timestep) {
    auto fluent_curr_z3 = convert_expression_to_z3(effect.fluent(), timestep);
    auto fluent_next_z3 = convert_expression_to_z3(effect.fluent(), timestep + 1);
    auto value_z3 = convert_expression_to_z3(effect.value(), timestep);
    
    if (!fluent_next_z3 || !value_z3 || !fluent_curr_z3) {
        std::cerr << "Error: Failed to convert effect fluent or value to Z3" << std::endl;
        return std::nullopt;
    }
    
    z3::expr effect_constraint = ctx_.bool_val(true);
    switch (effect.kind()) {
        case EffectExpression::Kind::ASSIGN:
            effect_constraint = (*fluent_next_z3 == *value_z3);
            break;
            
        case EffectExpression::Kind::INCREASE: {
            effect_constraint = (*fluent_next_z3 == *fluent_curr_z3 + *value_z3);
            break;
        }
        case EffectExpression::Kind::DECREASE: {
            effect_constraint = (*fluent_next_z3 == *fluent_curr_z3 - *value_z3);
            break;
        }
    }
    
    if (effect.is_conditional()) { // Handle conditional effects
        const Expression& condition = effect.condition();
        auto condition_z3 = convert_expression_to_z3(condition, timestep);
        if (condition_z3) {
            effect_constraint = z3::implies(*condition_z3, effect_constraint);
        }
    }
    
    return effect_constraint;
}

// Encoding steps
std::shared_ptr<z3::expr> GroundedEncoder::encode_initial_state() {
    z3::expr initial_state_formula = ctx_.bool_val(true);
    
    // Process each assignment in the initial state at timestep 0
    for (const auto& assignment : problem_.initial_state()) {
        auto fluent_expr = convert_expression_to_z3(assignment.fluent(), 0);
        auto value_expr = convert_expression_to_z3(assignment.value(), 0);
        
        if (!fluent_expr || !value_expr) {
            std::cerr << "Error: Failed to encode assignment in initial state" << std::endl;
            continue;
        }
        // Create and add equality constraint: fluent = value
        initial_state_formula = initial_state_formula && (*fluent_expr == *value_expr);
    }
    return std::make_shared<z3::expr>(initial_state_formula);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_actions(int t) {
    std::vector<z3::expr> action_constraints;
    
    for (const Action& action : problem_.actions()) {
        z3::expr action_var = variable_factory_.get_action_variable(action, t); 
        
        // Create precondition constraints: action_var => precondition
        if (action.has_precondition()) {
            auto z3_precond = convert_expression_to_z3(action.precondition(), t);
            if (z3_precond.has_value()) {
                // action_var => precondition
                action_constraints.push_back(z3::implies(action_var, z3_precond.value()));
            }
        }
        
        // Create effect constraints: action_var => effects
        if (!action.effects().empty()) {
            std::vector<z3::expr> effect_exprs;
            for (const Effect& effect : action.effects()) {
                auto z3_effect = convert_effect_to_z3(effect.effect_expression(), t);
                if (z3_effect.has_value()) {
                    effect_exprs.push_back(z3_effect.value());
                }
            }
            
            if (!effect_exprs.empty()) {
                // Create a flat conjunction using Z3's mk_and function
                z3::expr_vector effect_vector(ctx_);
                for (const auto& expr : effect_exprs) {
                    effect_vector.push_back(expr);
                }
                z3::expr effect_conjunction = z3::mk_and(effect_vector);
                // action_var => effect_conjunction
                action_constraints.push_back(z3::implies(action_var, effect_conjunction));
            }
        }
    }
    
    // Combine all action constraints with logical AND
    if (action_constraints.empty()) {
        auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
        return expr;
    }
    
    // Create a flat conjunction using Z3's mk_and function
    z3::expr_vector constraint_vector(ctx_);
    for (const auto& constraint : action_constraints) {
        constraint_vector.push_back(constraint);
    }
    z3::expr big_and = z3::mk_and(constraint_vector);
    
    return std::make_shared<z3::expr>(big_and);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_frames(int t) {
    std::vector<z3::expr> frame_axioms;
    
    // Iterate through all fluents in the EPC index
    for (const auto& [fluent, action_effects] : epc_index_) {
        // Get fluent variables at timesteps t and t+1
        auto fluent_t = convert_expression_to_z3(fluent, t);
        auto fluent_t_plus_1 = convert_expression_to_z3(fluent, t + 1);
        
        // Create the "fluent changed" condition: fluent^t != fluent^(t+1)
        z3::expr fluent_changed = (*fluent_t != *fluent_t_plus_1);
        
        // Build disjunction of all actions that can cause this change
        std::vector<z3::expr> action_terms;
        
        for (const auto& [action, effect_expr] : action_effects) {
            // Get the action variable at timestep t
            z3::expr action_var = variable_factory_.get_action_variable(*action, t);
            
            // For conditional effects: action_var && condition
            if (effect_expr->is_conditional()) {
                auto condition_z3 = convert_expression_to_z3(effect_expr->condition(), t);
                action_terms.push_back(action_var && *condition_z3); // condition /\ action_var
            } else {
                action_terms.push_back(action_var); // For unconditional effects: just the action variable
            }
        }
        
        // Create the action disjunction
        z3::expr action_disjunction = ctx_.bool_val(false);
        if (!action_terms.empty()) {
            // Create a flat disjunction using Z3's mk_or function
            z3::expr_vector action_vector(ctx_);
            for (const auto& term : action_terms) {
                action_vector.push_back(term);
            }
            action_disjunction = z3::mk_or(action_vector);
        }
        
        // Create the frame axiom: fluent_changed -> action_disjunction
        // This is equivalent to: (fluent^t != fluent^(t+1)) -> (a1 || (epc2 && a2) || a3 || ...)
        z3::expr frame_axiom = z3::implies(fluent_changed, action_disjunction);
        frame_axioms.push_back(frame_axiom);
    }
    
    // Combine all frame axioms with logical AND
    if (frame_axioms.empty()) {
        auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
        return expr;
    }
    
    // Create a flat conjunction using Z3's mk_and function
    z3::expr_vector frame_vector(ctx_);
    for (const auto& axiom : frame_axioms) {
        frame_vector.push_back(axiom);
    }
    z3::expr big_and = z3::mk_and(frame_vector);
    return std::make_shared<z3::expr>(big_and);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_goal(int t) {
    // Retrieve goals from the problem
    const auto& goals = problem_.goals();
    
    if (goals.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true)); // If no goals, vacuously satisfied
    }
    
    // Convert each goal expression to Z3 formula and collect them
    std::vector<z3::expr> goal_formulas;
    goal_formulas.reserve(goals.size());
    
    for (const auto& goal : goals) {
        auto z3_goal = convert_expression_to_z3(goal.goal_expression(), t);
        if (z3_goal) {
            goal_formulas.push_back(*z3_goal);
            std::cout << "Goal encoded: " << *z3_goal << std::endl;
        } else {
            std::cerr << "Error: Failed to encode goal expression: " << goal.to_string() << std::endl;
        }
    }
    // Combine all goal formulas with logical AND
    if (goal_formulas.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    
    // Create a flat conjunction using Z3's mk_and function
    z3::expr_vector goal_vector(ctx_);
    for (const auto& goal : goal_formulas) {
        goal_vector.push_back(goal);
    }
    z3::expr goal_conjunction = z3::mk_and(goal_vector);
    return std::make_shared<z3::expr>(goal_conjunction);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_parallelism(int t) {
    // TODO: Implement parallelism constraints for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

void GroundedEncoder::print_epc_index(const std::string& context) const {
    std::cout << "\n===== EPC Index: " << context << " =====" << std::endl;
    
    if (epc_index_.empty()) {
        std::cout << "(empty)" << std::endl;
    } else {
        for (const auto& [fluent, action_effects] : epc_index_) {
            std::cout << "Fluent: " << fluent.to_string() << std::endl;
            for (const auto& [action, eff_expr] : action_effects) {
                std::cout << "  Modified by Action: " << action->name() << " | Effect: " << eff_expr->to_string() << std::endl;
            }
        }
    }
    std::cout << "===== End EPC Index =====" << std::endl << std::endl;
}

void GroundedEncoder::build_epc_index() {

    // Local helper function to index effect fluents in case we need to handle complex effects
    // in the future like nested quantified/conditional effects. As of now, I think
    // the limitation is UP's though ...
    auto index_effect_fluents = [this](const Action* action, const EffectExpression* eff_expr) {
        // Index the direct effect
        const Expression& fluent = eff_expr->fluent();
        epc_index_[fluent].emplace_back(action, eff_expr);

        // Recursively handle quantified effects (forall)
        // In standard ADL/PDDL, quantified effects are represented as EffectExpressions with forall_variables_ non-empty.
        // If you extend EffectExpression to support a list of sub-effects, recurse into them here.
        // For now, we assume the current EffectExpression is the only effect, so nothing to do.

        // Recursively handle conditional effects (when ...)
        // In standard ADL/PDDL, the condition is a logical formula, not an effect, so nothing to do.
        // If you extend EffectExpression to support sub-effects in the value or condition, recurse into them here.
        // For now, we assume the value is not an EffectExpression, so nothing to do.
    };

    epc_index_.clear();
    for (const auto& action : problem_.actions()) {
        for (const auto& effect : action.effects()) {
            const EffectExpression& eff_expr = effect.effect_expression();
            index_effect_fluents(&action, &eff_expr);
        }
    }
    
    // Print the index for debugging (uncomment when needed)
    // print_epc_index("After building EPC index");
}

} // namespace planmt