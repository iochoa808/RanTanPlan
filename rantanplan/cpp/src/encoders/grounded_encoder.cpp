#include "grounded_encoder.hpp"
#include "parallelism/interference_analysis.hpp"
#include "../util/stats.hpp"
#include "../config/config.hpp"
#include <iostream>
#include <functional>

namespace rantanplan {

// Constructor
GroundedEncoder::GroundedEncoder(const Problem& problem, z3::context& ctx)
    : problem_(problem), ctx_(ctx), variable_factory_(ctx), grounded_visitor_(ctx_, &problem_, &variable_factory_) {
    layers_encoded_ = -1;
    build_epc_index();
}

z3::expr GroundedEncoder::convert_expr_id_to_z3(ExprID id, int timestep) {
    return grounded_visitor_.convert_from_pool(id, timestep);
}

// Helper function to convert effect to Z3 constraint using visitor
z3::expr GroundedEncoder::convert_effect_to_z3(const EffectExpression& effect, int timestep) {
    z3::expr fluent_curr_z3 = convert_expr_id_to_z3(effect.fluent_id(), timestep);
    z3::expr fluent_next_z3 = convert_expr_id_to_z3(effect.fluent_id(), timestep + 1);
    z3::expr value_z3 = convert_expr_id_to_z3(effect.value_id(), timestep);

    z3::expr effect_constraint = ctx_.bool_val(true);
    switch (effect.kind()) {
        case EffectExpression::Kind::ASSIGN:
            effect_constraint = (fluent_next_z3 == value_z3);
            break;
        case EffectExpression::Kind::INCREASE:
            effect_constraint = (fluent_next_z3 == fluent_curr_z3 + value_z3);
            break;
        case EffectExpression::Kind::DECREASE:
            effect_constraint = (fluent_next_z3 == fluent_curr_z3 - value_z3);
            break;
    }

    if (effect.is_conditional()) {
        z3::expr condition_z3 = convert_expr_id_to_z3(effect.condition_id(), timestep);
        effect_constraint = z3::implies(condition_z3, effect_constraint);
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
    auto& stats = Stats::instance();
    
    // Process each assignment in the initial state at timestep 0
    for (const auto& assignment : problem_.initial_state()) {
        z3::expr fluent_expr = convert_expr_id_to_z3(assignment.fluent_id(), 0);
        z3::expr value_expr = convert_expr_id_to_z3(assignment.value_id(), 0);
        initial_state.push_back(fluent_expr == value_expr);
    }
    
    // Collect statistics
    stats.set("encoder.initial_constraints", initial_state.size());
    
    // Combine all initial state constraints with logical AND
    if (initial_state.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    z3::expr initial_state_formula = z3::mk_and(initial_state);
    return std::make_shared<z3::expr>(initial_state_formula);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_actions(int t) {
    z3::expr_vector action_constraints(ctx_);
    auto& stats = Stats::instance();

    for (const Action& action : problem_.actions()) {
        z3::expr action_var = variable_factory_.get_action_variable(action, t); 
        
        // if the action has no effects, we skip it as it cannot change any fluents
        if (!action.effects().empty()) {

            // Create precondition constraints: action_var => precondition
            if (action.has_precondition()) {
                z3::expr z3_precond = convert_expr_id_to_z3(action.precondition_id(), t);
                action_constraints.push_back(z3::implies(action_var, z3_precond));
            }

            // Create effect constraints: action_var => effects
            z3::expr_vector effect_exprs(ctx_);
            for (const Effect& effect : action.effects()) {
                effect_exprs.push_back(convert_effect_to_z3(effect.effect_expression(), t));
            }
            z3::expr effect_conjunction = z3::mk_and(effect_exprs);
            // action_var => effect_conjunction
            //std::cout << "eff:" << action_var.to_string() << " -> " << effect_conjunction.to_string() << std::endl;
            action_constraints.push_back(z3::implies(action_var, effect_conjunction));
        }
    }
    
    // Collect statistics
    stats.add("encoder.action_constraints", action_constraints.size());
    
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
    auto& stats = Stats::instance();
    
    // Iterate through all grounded fluents and look up in EPC index
    for (ExprID eid : problem_.grounded_fluents()) {
        auto epc_it = epc_index_.find(eid);
        if (epc_it == epc_index_.end()) continue;
        const auto& action_effects = epc_it->second;

        // Get fluent variables at timesteps t and t+1
        z3::expr fluent_t = convert_expr_id_to_z3(eid, t);
        z3::expr fluent_t_plus_1 = convert_expr_id_to_z3(eid, t + 1);

        // Create the "fluent changed" condition: fluent^t != fluent^(t+1)
        z3::expr fluent_changed = (fluent_t != fluent_t_plus_1);
        
        // Build disjunction of all actions that can cause this change
        std::vector<z3::expr> action_terms;
        
        for (const auto& [action, effect_expr] : action_effects) {
            // Get the action variable at timestep t
            z3::expr action_var = variable_factory_.get_action_variable(*action, t);
            
            // For conditional effects: action_var && condition
            if (effect_expr->is_conditional()) {
                z3::expr condition_z3 = convert_expr_id_to_z3(effect_expr->condition_id(), t);
                action_terms.push_back(action_var && condition_z3); // condition /\ action_var
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
    
    // Collect statistics
    stats.add("encoder.frame_axioms", frame_axioms.size());
    
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
    auto& stats = Stats::instance();
    
    if (goals.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true)); // If no goals, vacuously satisfied
    }
    
    // Convert each goal expression to Z3 formula and collect them
    std::vector<z3::expr> goal_formulas;
    goal_formulas.reserve(goals.size());
    
    for (const auto& goal : goals) {
        goal_formulas.push_back(convert_expr_id_to_z3(goal.goal_id(), t));
    }
    
    // Collect statistics
    stats.set("encoder.goal_constraints", goal_formulas.size());
    
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
std::shared_ptr<z3::expr> GroundedEncoder::encode_prefix_monotone(int t) {
    // Build: (∀a. ¬a@t)  →  (∀a. ¬a@(t+1))
    // i.e., if no action fires at t, then no action may fire at t+1.
    // Chained across all t during search, this front-loads all active steps
    // to the prefix of the plan (0..k-1), with empty steps (k..h-1) at the end.
    // Frame axioms then propagate the goal state through the empty suffix,
    // so a single goal literal placed at horizon h witnesses any plan of
    // length k <= h — enabling the horizon schedule's single-literal batching.
    z3::expr_vector no_action_t(ctx_), no_action_t1(ctx_);
    for (const Action& a : problem_.actions()) {
        no_action_t.push_back(!variable_factory_.get_action_variable(a, t));
        no_action_t1.push_back(!variable_factory_.get_action_variable(a, t + 1));
    }
    if (no_action_t.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    return std::make_shared<z3::expr>(
        z3::implies(z3::mk_and(no_action_t), z3::mk_and(no_action_t1))
    );
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_symmetries(int t) {
    auto& stats = Stats::instance();

    if (symmetry_data_.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }

    z3::expr_vector symmetry_constraints(ctx_);
    int ordering_constraints_count = 0;

    for (const auto& sym : symmetry_data_) {
        // LHS: all variable pairs have the same value (symmetric state)
        z3::expr_vector variable_equality_constraints(ctx_);
        for (const auto& [var1_eid, var2_eid] : sym.variable_pairs) {
            z3::expr var1_z3 = convert_expr_id_to_z3(var1_eid, t);
            z3::expr var2_z3 = convert_expr_id_to_z3(var2_eid, t);
            variable_equality_constraints.push_back(var1_z3 == var2_z3);
        }

        // RHS: lexicographic ordering on action pairs
        z3::expr_vector action_ordering_constraints(ctx_);
        for (const auto& action_pair : sym.action_pairs) {
            z3::expr action1_var = variable_factory_.get_action_variable(*action_pair.action1, t);
            z3::expr action2_var = variable_factory_.get_action_variable(*action_pair.action2, t);

            std::string name1 = variable_factory_.get_action_var_name(*action_pair.action1);
            std::string name2 = variable_factory_.get_action_var_name(*action_pair.action2);
            z3::expr ordering_constraint = (name1 < name2) ?
                z3::implies(action1_var, action2_var) :
                z3::implies(action2_var, action1_var);
            action_ordering_constraints.push_back(ordering_constraint);
            ordering_constraints_count++;
        }

        // (all variables are symmetric) => (lexicographic ordering on actions)
        if (!variable_equality_constraints.empty() && !action_ordering_constraints.empty()) {
            z3::expr lhs = z3::mk_and(variable_equality_constraints);
            z3::expr rhs = z3::mk_and(action_ordering_constraints);
            symmetry_constraints.push_back(z3::implies(lhs, rhs));
        }
    }

    stats.add("encoder.symmetry_ordering_constraints", ordering_constraints_count);

    if (symmetry_constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    return std::make_shared<z3::expr>(z3::mk_and(symmetry_constraints));
}

void GroundedEncoder::set_parallelism_strategy(std::unique_ptr<ParallelismStrategy> strategy) {
    parallelism_strategy_ = std::move(strategy);
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
        for (const auto& [eid, action_effects] : epc_index_) {
            std::cout << "Fluent: " << problem_.pool().to_string(eid) << std::endl;
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
        epc_index_[eff_expr->fluent_id()].emplace_back(action, eff_expr);
    };

    epc_index_.clear();

    // Use the systematic grounded fluent collection from the problem
    // This ensures ALL grounded fluents get frame axioms, including those from
    // initial state, action effects, preconditions, and goals
    for (ExprID eid : problem_.grounded_fluents()) {
        epc_index_[eid] = std::vector<std::pair<const Action*, const EffectExpression*>>();
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

Plan GroundedEncoder::extract_plan(const z3::model& model, int max_timestep) const {
    Plan plan;

    std::cout << "Extracting plan from Z3 model with " << model.size() << " variable assignments" << std::endl;

    // Use type-safe capability query instead of string comparison
    bool is_parallel = get_parallelism_strategy()->allows_concurrent_actions();
    
    // Iterate through each timestep
    for (int t = 0; t < max_timestep; ++t) {
        if (is_parallel) {
            // Extract and order parallel actions for this timestep
            std::vector<const Action*> parallel_actions = extract_parallel_actions_at_timestep(model, t);
            
            if (!parallel_actions.empty()) {
                std::vector<const Action*> ordered_actions = topologically_sort_actions(parallel_actions);
                // Add ordered actions to plan
                for (const Action* action : ordered_actions) {
                    plan.add_action(action);
                }
            }
        } else {
            // Original sequential extraction logic  
            for (const Action& grounded_action : problem_.actions()) {
                z3::expr action_var = variable_factory_.get_action_variable(grounded_action, t);
                z3::expr action_value = model.eval(action_var, true); // Use model completion
                
                if (action_value.is_true()) {
                    plan.add_action(&grounded_action);
                    break; // Only one action in sequential mode
                }
            }
        }
    }
    return plan;
}

std::vector<const Action*> GroundedEncoder::extract_parallel_actions_at_timestep(
    const z3::model& model, int timestep) const {
    
    std::vector<const Action*> parallel_actions;
    
    for (const Action& grounded_action : problem_.actions()) {
        try {
            z3::expr action_var = variable_factory_.get_action_variable(grounded_action, timestep);
            z3::expr action_value = model.eval(action_var, true);
            
            if (action_value.is_true()) {
                parallel_actions.push_back(&grounded_action);
            }
        } catch (const std::exception&) {
            // Skip actions whose variables don't exist
        }
    }
    
    return parallel_actions;
}

std::vector<const Action*> GroundedEncoder::topologically_sort_actions(
    const std::vector<const Action*>& actions) const {
    
    if (actions.size() <= 1) {
        std::vector<const Action*> result;
        result.reserve(actions.size());
        for (const Action* action : actions) {
            result.push_back(action);
        }
        return result; // No sorting needed
    }
    
    // We always have a strategy and analyzer available
    const ParallelismStrategy* strategy = get_parallelism_strategy();
    const InterferenceAnalysis* analyzer = strategy->get_interference_analyzer();
    
    return analyzer->topological_sort_actions(actions);
}

} // namespace rantanplan