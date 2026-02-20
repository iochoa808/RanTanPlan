#include "grounded_encoder.hpp"
#include "parallelism/interference_analysis.hpp"
#include "../util/stats.hpp"
#include "../symmetries/smt_symmetry_checker.hpp"
#include "../config/config.hpp"
#include <iostream>
#include "problem/visitors/print_visitor.hpp"
#include "problem/visitors/expression_visitor.hpp"
#include <functional>

namespace rantanplan {

// Constructor
GroundedEncoder::GroundedEncoder(const Problem& problem, z3::context& ctx)
    : problem_(problem), ctx_(ctx), variable_factory_(ctx), grounded_visitor_(ctx_, &problem_, &variable_factory_) {
    layers_encoded_ = -1;
    build_epc_index();
    analyze_symmetries();

    // Parallelism strategy will be set by the caller
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
    auto& stats = Stats::instance();
    
    // Process each assignment in the initial state at timestep 0
    for (const auto& assignment : problem_.initial_state()) {
        auto fluent_expr = convert_expression_to_z3(assignment.fluent(), 0);
        auto value_expr = convert_expression_to_z3(assignment.value(), 0);
        initial_state.push_back(*fluent_expr == *value_expr);
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
        auto z3_goal = convert_expression_to_z3(goal.goal_expression(), t);
        if (z3_goal) {
            goal_formulas.push_back(*z3_goal);
        } else {
            std::cerr << "Error: Failed to encode goal expression: " << goal.to_string() << std::endl;
        }
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
    auto& config = Config::instance();
    auto& stats = Stats::instance();
    
    // Only run symmetry breaking if enabled or something to break
    if (!config.symmetry.detect_symmetries || detected_symmetries_.empty()) {
            return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
   
    z3::expr_vector symmetry_constraints(ctx_);
    int ordering_constraints_count = 0;
    
    // For each detected symmetry, create symmetry breaking constraints
    for (size_t i = 0; i < detected_symmetries_.size(); i++) {
        const auto& swap = detected_symmetries_[i];
        // Get variable and action pairs for this symmetry from cached checker
        auto variable_pairs = symmetry_checker_->get_variable_pairs_for_swap(swap.obj1_name, swap.obj2_name);
        auto action_pairs = symmetry_checker_->get_action_pairs_for_swap(swap.obj1_name, swap.obj2_name);
        
        // Create the symmetry breaking constraint for this timestep
        // LHS: Check if all variable pairs have the same value (symmetric state)
        z3::expr_vector variable_equality_constraints(ctx_);
        
        for (const auto& [var1_ptr, var2_ptr] : variable_pairs) {
            auto var1_z3 = convert_expression_to_z3(*var1_ptr, t);
            auto var2_z3 = convert_expression_to_z3(*var2_ptr, t);
            variable_equality_constraints.push_back(*var1_z3 == *var2_z3);
        }
        
        // RHS: Lexicographic ordering constraint on action pairs
        z3::expr_vector action_ordering_constraints(ctx_);
        // Generate action ordering constraints 
        for (size_t j = 0; j < action_pairs.size(); j++) {
            const auto& action_pair = action_pairs[j];

            // Get action variables
            z3::expr action1_var = variable_factory_.get_action_variable(*action_pair.action1, t);
            z3::expr action2_var = variable_factory_.get_action_variable(*action_pair.action2, t);
            
            // Create ordering constraint
            std::string name1 = variable_factory_.get_action_var_name(*action_pair.action1);
            std::string name2 = variable_factory_.get_action_var_name(*action_pair.action2);
            z3::expr ordering_constraint = (name1 < name2) ?
                z3::implies(action1_var, action2_var) : // If action1 should come first lexicographically
                z3::implies(action2_var, action1_var);  // Otherwise action2 should come first
            action_ordering_constraints.push_back(ordering_constraint);
            ordering_constraints_count++;
        }
        
        // Create the complete symmetry breaking constraint:
        // (all variables are symmetric) => (lexicographic ordering on actions)
        if (!variable_equality_constraints.empty() && !action_ordering_constraints.empty()) {
            z3::expr lhs = z3::mk_and(variable_equality_constraints);
            z3::expr rhs = z3::mk_and(action_ordering_constraints);
            z3::expr symmetry_breaking_constraint = z3::implies(lhs, rhs);
            symmetry_constraints.push_back(symmetry_breaking_constraint);
        }
    } // end for loop over object_swaps
    
    // Record the statistic
    stats.add("encoder.symmetry_ordering_constraints", ordering_constraints_count);
    
    // Combine all symmetry constraints
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

    // Use the systematic grounded fluent collection from the problem
    // This ensures ALL grounded fluents get frame axioms, including those from
    // initial state, action effects, preconditions, and goals
    for (const Expression& fluent : problem_.grounded_fluents()) {
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

void GroundedEncoder::analyze_symmetries() {
    auto& config = Config::instance();

    // Only analyze symmetries if symmetry detection is enabled
    if (!config.symmetry.detect_symmetries) {
        if (config.is_debug()) {
            std::cout << "Symmetry detection disabled - skipping analysis" << std::endl;
        }
        return;
    }

    // Create symmetry checker and perform analysis once
    symmetry_checker_ = std::make_unique<SMTSymmetryChecker>(&problem_, ctx_);
    detected_symmetries_ = symmetry_checker_->detect_all_object_swaps();

}


} // namespace rantanplan