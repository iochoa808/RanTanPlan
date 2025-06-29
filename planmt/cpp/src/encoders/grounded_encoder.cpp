#include "grounded_encoder.h"
#include <iostream>
#include "problem/visitors/print_visitor.h"
#include "problem/visitors/expression_visitor.h"
#include <functional>

namespace planmt {

// Constructor
GroundedEncoder::GroundedEncoder(const Problem& problem, z3::context& ctx)
    : problem_(problem), ctx_(ctx), symbol_table_(), smt_visitor_(ctx_, symbol_table_, &problem_), grounded_visitor_(ctx_, this, &problem_) {
    // Initialize storage for state and action variables
    state_vars_.clear();
    action_vars_.clear();
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
        z3::expr action_var = get_action_var(action, t); 
        
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
                z3::expr effect_conjunction = effect_exprs[0];
                for (size_t i = 1; i < effect_exprs.size(); ++i) {
                    effect_conjunction = effect_conjunction && effect_exprs[i];
                }
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
    
    z3::expr big_and = action_constraints[0];
    for (size_t i = 1; i < action_constraints.size(); ++i) {
        big_and = big_and && action_constraints[i];
    }
    
    return std::make_shared<z3::expr>(big_and);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_frames(int t) {
    // TODO: Implement frame axioms encoding for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
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
    z3::expr goal_conjunction = goal_formulas[0];
    for (size_t i = 1; i < goal_formulas.size(); ++i) {
        goal_conjunction = goal_conjunction && goal_formulas[i];
    }
    return std::make_shared<z3::expr>(goal_conjunction);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_parallelism(int t) {
    // TODO: Implement parallelism constraints for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

// Private helper methods
z3::expr GroundedEncoder::get_fluent_var(const Fluent& fluent, int t) {
    std::string var_name = get_smt_var_name(fluent, t);
    
    // Ensure we have enough timesteps allocated
    while (static_cast<int>(state_vars_.size()) <= t) {
        state_vars_.emplace_back();
    }
    
    auto& timestep_vars = state_vars_[t];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        // Create new variable with correct type based on fluent's value type
        z3::expr new_var = create_typed_variable(fluent, var_name);
        timestep_vars[var_name] = std::make_shared<z3::expr>(new_var);
        return new_var;
    }
    return *(it->second);
}

z3::expr GroundedEncoder::get_action_var(const Action& action, int t) {
    std::string var_name = get_smt_var_name(action, t);
    
    // Ensure we have enough timesteps allocated
    while (static_cast<int>(action_vars_.size()) <= t) {
        action_vars_.emplace_back();
    }
    
    auto& timestep_vars = action_vars_[t];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        // Create new variable using the visitor to ensure consistency
        z3::expr new_var = smt_visitor_.create_bool_variable(var_name);
        timestep_vars[var_name] = std::make_shared<z3::expr>(new_var);
        return new_var;
    }
    return *(it->second);
}

std::string GroundedEncoder::get_smt_var_name(const Fluent& fluent) const {
    std::string name = fluent.name();
    // Add parameter values from the fluent's embedded parameters
    for (const auto& param : fluent.parameters()) {
        name += "_" + param.name();
    }
    return name;
}

std::string GroundedEncoder::get_smt_var_name(const Fluent& fluent, int t) const {
    return get_smt_var_name(fluent) + "_" + std::to_string(t);
}

std::string GroundedEncoder::get_smt_var_name(const Action& action) const {
    std::string name = action.name();
    // Add parameter values from the action's embedded parameters
    for (const auto& param : action.parameters()) {
        name += "_" + param.name();
    }
    return name;
}

std::string GroundedEncoder::get_smt_var_name(const Action& action, int t) const {
    return get_smt_var_name(action) + "_" + std::to_string(t);
}

z3::expr GroundedEncoder::create_typed_variable(const Fluent& fluent, const std::string& var_name) {
    // Use the new type checking methods from Fluent class
    if (fluent.is_bool_fluent()) {
        // Boolean fluents (predicates)
        return smt_visitor_.create_bool_variable(var_name);
    } else if (fluent.is_int_fluent()) {
        // Integer fluents
        return smt_visitor_.create_int_variable(var_name);
    } else if (fluent.is_real_fluent()) {
        // Real fluents
        return smt_visitor_.create_real_variable(var_name);
    } else if (fluent.is_object_fluent()) {
        // Object fluents are typically mapped to integers in SMT encoding
        return smt_visitor_.create_int_variable(var_name);
    } else {
        // For unknown types, default to integer
        return smt_visitor_.create_int_variable(var_name);
    }
}

void GroundedEncoder::print_symbol_table(const std::string& context) const {
    std::cout << "\n===== Symbol Table: " << context << " =====" << std::endl;
    std::cout << "Total symbols: " << symbol_table_.size() << std::endl;
    
    if (symbol_table_.empty()) {
        std::cout << "(empty)" << std::endl;
    } else {
        for (const auto& [name, z3_object] : symbol_table_) {
            std::cout << "  " << name << " -> ";
            if (std::holds_alternative<z3::expr>(z3_object)) {
                const auto& expr = std::get<z3::expr>(z3_object);
                std::cout << "expr: " << expr << " (sort: " << expr.get_sort() << ")";
            } else if (std::holds_alternative<z3::func_decl>(z3_object)) {
                const auto& func_decl = std::get<z3::func_decl>(z3_object);
                std::cout << "func_decl: " << func_decl << " (arity: " << func_decl.arity() << ")";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "===== End Symbol Table =====" << std::endl << std::endl;
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