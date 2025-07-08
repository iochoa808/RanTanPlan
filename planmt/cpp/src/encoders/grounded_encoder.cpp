#include "grounded_encoder.h"
#include "parallelism/parallelism_strategies.h"
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
    
    // Initialize with sequential semantics by default
    set_parallelism_strategy(ParallelismType::SEQUENTIAL);
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
        std::cerr << "  fluent_curr_z3: " << (fluent_curr_z3 ? fluent_curr_z3->to_string() : "null") << " (from: " << effect.fluent().to_string() << ")" << std::endl;
        std::cerr << "  fluent_next_z3: " << (fluent_next_z3 ? fluent_next_z3->to_string() : "null") << " (from: " << effect.fluent().to_string() << ")" << std::endl;
        std::cerr << "  value_z3: " << (value_z3 ? value_z3->to_string() : "null") << " (from: " << effect.value().to_string() << ")" << std::endl;
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

/**
 * @brief Encodes the initial state constraints at timestep 0
 * 
 * Note: The Python side takes responsibility for initializing all fluents 
 * with default values, so here we can just iterate over the initial state.
 */
std::shared_ptr<z3::expr> GroundedEncoder::encode_initial_state() {
   z3::expr_vector initial_state(ctx_);
    
    // Process each assignment in the initial state at timestep 0
    for (const auto& assignment : problem_.initial_state()) {
        auto fluent_expr = convert_expression_to_z3(assignment.fluent(), 0);
        auto value_expr = convert_expression_to_z3(assignment.value(), 0);
        initial_state.push_back(*fluent_expr == *value_expr);
    }
    
    // Combine all initial state constraints with logical AND
    if (initial_state.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    z3::expr initial_state_formula = z3::mk_and(initial_state);
    return std::make_shared<z3::expr>(initial_state_formula);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_actions(int t) {
    z3::expr_vector action_constraints(ctx_);

    for (const Action& action : problem_.actions()) {
        z3::expr action_var = variable_factory_.get_action_variable(action, t); 
        
        // if the action has no effects, we skip it as it cannot change any fluents
        if (!action.effects().empty()) {

            // Create precondition constraints: action_var => precondition
            if (action.has_precondition()) {
                std::optional<z3::expr> z3_precond = convert_expression_to_z3(action.precondition(), t);
                //std::cout << "pre:" << action_var.to_string() << " -> " << z3_precond.value().to_string() << std::endl;
                action_constraints.push_back(z3::implies(action_var, z3_precond.value())); // we assume precondition is valid
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

/**
 * @brief Encodes frame axioms for a specific time step
 *
 * Frame axioms ensure that fluents only change when explicitly caused by
 * actions.  For each fluent in the EPC index, adds a constraint that says: "if a
 * fluent changes between time t and t+1, then at least one action that can
 * cause this change must be executed at time t".
 *
 * (at_robot_A^t != at_robot_A^(t+1)) -> (move_A_to_B^t || (Effect_precondition^t && conditional_action^t))
 *
 * @param t The current time step for which to encode frame axioms
 * @return A shared pointer to a Z3 expression representing the conjunction of all frame
 * axioms for the transition from time t to t+1. Returns true if no fluents
 * exist in the EPC index.
 */
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
        //std::cout << "Frame axiom for fluent: " << fluent_changed.to_string() << std::endl;
        //std::cout << "  Action disjunction: " << action_disjunction.to_string() << std::endl;
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
    return parallelism_strategy_->encode_parallelism(t);
}

void GroundedEncoder::set_parallelism_strategy(ParallelismType type) {
    switch (type) {
        case ParallelismType::SEQUENTIAL:
            parallelism_strategy_ = std::make_unique<SequentialSemantics>();
            break;
        case ParallelismType::FORALL:
            parallelism_strategy_ = std::make_unique<ForallSemantics>();
            break;
        case ParallelismType::EXISTS:
            parallelism_strategy_ = std::make_unique<ExistsSemantics>();
            break;
    }
    // Initialize the strategy with problem context
    parallelism_strategy_->initialize(problem_, ctx_, variable_factory_);
}

std::string GroundedEncoder::get_parallelism_strategy_name() const {
    if (parallelism_strategy_) {
        return parallelism_strategy_->get_name();
    }
    return "Unknown";
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
    
    // First, populate the EPC index with ALL fluents from the initial state
    // This ensures every fluent gets frame axioms, even those never modified by actions
    for (const auto& assignment : problem_.initial_state()) {
        const Expression& fluent = assignment.fluent();
        epc_index_[fluent] = std::vector<std::pair<const Action*, const EffectExpression*>>();
    }
    
    // Then, add action effects to the fluents that can be modified
    for (const auto& action : problem_.actions()) {
        for (const auto& effect : action.effects()) {
            const EffectExpression& eff_expr = effect.effect_expression();
            index_effect_fluents(&action, &eff_expr);
        }
    }
    
    // Print the index for debugging
    //print_epc_index("After building EPC index");
}

} // namespace planmt