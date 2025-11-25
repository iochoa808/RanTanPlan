#include "r2e_grounded_encoder.hpp"
#include "../util/stats.hpp"
#include "../config/config.hpp"
#include <algorithm>
#include <cassert>

// R2E encoder implementing "Relaxed ∃-Step Plans in Planning as SMT" by Bofill, Espasa, and Villaret

namespace rantanplan {

R2EGroundedEncoder::R2EGroundedEncoder(const Problem& problem, z3::context& ctx, ActionOrdering ordering)
    : GroundedEncoder(problem, ctx), action_ordering_(ordering) {
    build_action_ordering();
    collect_all_state_variables();
    build_variable_modifiers();
    build_rho_mappings();
    build_prev_mappings();
    
    // Uncomment the line below to enable debug output during construction
    //debug_print_structures();
}

void R2EGroundedEncoder::build_action_ordering() {
    global_action_order_.clear();
    global_action_order_.reserve(problem_.actions().size());
    
    switch (action_ordering_) {
        case ActionOrdering::DEC:
            // Add actions in the order they appear in the problem
            for (const Action& action : problem_.actions()) {
                global_action_order_.push_back(&action);
            }
            break;
        case ActionOrdering::ARPG:
            // Extract ordering from ARPG layers
            global_action_order_ = extract_arpg_ordering();
            break;
    }
    
    Stats::instance().set("encoder.r2e.total_actions", global_action_order_.size());
}

std::vector<const Action*> R2EGroundedEncoder::extract_arpg_ordering() {
    // Create ARPG and construct the graph
    ARPG arpg(problem_);
    bool goal_reachable = arpg.construct_graph();
    
    if (!goal_reachable) {
        std::cout << "Warning: ARPG could not reach the goal. Using declaration order as fallback." << std::endl;
        std::vector<const Action*> fallback_order;
        for (const Action& action : problem_.actions()) {
            fallback_order.push_back(&action);
        }
        return fallback_order;
    }
    
    // Use detailed ARPG supporter ordering to get high-quality action ordering
    std::vector<const Action*> arpg_ordered_actions = arpg.get_action_ordering();

    // Get detailed supporter ordering for analysis
    auto supporter_ordering = arpg.get_supporter_ordering();

    std::cout << "ARPG ordering: " << supporter_ordering.size() << " supporters across "
              << arpg.get_num_iterations() << " iterations, "
              << arpg_ordered_actions.size() << " unique actions" << std::endl;

    // Print detailed iteration-by-iteration ordering for debugging
    /*
    std::unordered_map<int, std::vector<std::string>> iteration_actions;
    for (const auto& info : supporter_ordering) {
        iteration_actions[info.iteration].push_back(info.source_action.name());
    }

    for (const auto& [iteration, actions] : iteration_actions) {
        std::cout << "  Iteration " << iteration << ": ";
        for (size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << actions[i];
        }
        std::cout << std::endl;
    }
    */
    
    return arpg_ordered_actions;
}


void R2EGroundedEncoder::collect_all_state_variables() {
    all_state_variables_.clear();
    // Use the systematic grounded fluent collection from the problem
    // This ensures we capture ALL grounded fluents that appear anywhere in the problem
    for (const Expression& fluent : problem_.grounded_fluents()) {
        all_state_variables_.insert(fluent);
    }
}

void R2EGroundedEncoder::build_variable_modifiers() {
    variable_modifiers_.clear();
    
    // Collect all variables and their modifying actions IN GLOBAL ORDER
    for (const Action* action : global_action_order_) {
        for (const Effect& effect : action->effects()) {
            const Expression& variable = effect.effect_expression().fluent();
            variable_modifiers_[variable].push_back(action);
        }
    }
    
    // No need to sort - already in global order!
    // Just remove duplicates while preserving order
    for (auto& [variable, actions] : variable_modifiers_) {
        actions.erase(std::unique(actions.begin(), actions.end()), actions.end());
    }
    
    Stats::instance().set("encoder.r2e.modified_variables", variable_modifiers_.size());
}

void R2EGroundedEncoder::build_rho_mappings() {
    rho_x_.clear();
    
    for (const auto& [variable, modifying_actions] : variable_modifiers_) {
        std::vector<int>& rho = rho_x_[variable];
        rho.resize(modifying_actions.size() + 1);  // +1 for index 0
        
        // ρx(0) = 0 (added for notational convenience)
        rho[0] = 0;
        
        // For each action that modifies this variable, find its position in global ordering
        for (size_t i = 0; i < modifying_actions.size(); ++i) {
            const Action* action = modifying_actions[i];
            
            // Use helper method to get global index
            int global_index = get_global_action_index(action);
            rho[i + 1] = global_index;
        }
    }
}

void R2EGroundedEncoder::build_prev_mappings() {
    prev_x_.clear();
    
    for (const auto& [variable, modifying_actions] : variable_modifiers_) {
        std::vector<int>& prev = prev_x_[variable];
        prev.resize(global_action_order_.size() + 1);  // +1 for 1-based indexing
        
        // For each action in the global ordering
        for (size_t i = 0; i < global_action_order_.size(); ++i) {
            const Action* current_action = global_action_order_[i];
            int action_index = i + 1;  // Convert to 1-based indexing
            
            // Find the last action before current_action that modifies this variable
            const std::vector<int>& rho = rho_x_[variable];
            int prev_index = 0;  // Default to 0 if no previous action
            
            for (size_t j = 1; j < rho.size(); ++j) {  // Skip rho[0] = 0
                if (rho[j] < action_index) {
                    prev_index = rho[j];
                } else {
                    break;  // Actions are ordered, so we can stop here
                }
            }
            
            prev[action_index] = prev_index;
        }
    }
}

std::shared_ptr<z3::expr> R2EGroundedEncoder::encode_actions(int t) {
    z3::expr_vector all_constraints(ctx_);
    
    // Add main R2E constraints (equations 1-4 from paper)
    std::shared_ptr<z3::expr> precond = encode_precondition_constraints(t);
    if (precond) all_constraints.push_back(*precond);

    std::shared_ptr<z3::expr> effects = encode_effect_constraints(t);  // Now includes carry-forward logic
    if (effects) all_constraints.push_back(*effects);

    std::shared_ptr<z3::expr> linking = encode_linking_constraints(t);
    if (linking) all_constraints.push_back(*linking);
    
    Stats::instance().add("encoder.r2e.action_constraints", all_constraints.size());
    
    return all_constraints.empty() ? 
        std::make_shared<z3::expr>(ctx_.bool_val(true)) :
        std::make_shared<z3::expr>(z3::mk_and(all_constraints));
}

std::shared_ptr<z3::expr> R2EGroundedEncoder::encode_frames(int t) {
    // R2E has built-in frame semantics through chain variables and linking constraints
    // No additional frame axioms are needed
    return std::make_shared<z3::expr>(ctx_.bool_val(true));
}

std::shared_ptr<z3::expr> R2EGroundedEncoder::encode_parallelism(int t) {
    // R2E has built-in parallelism semantics, so we return true (no additional constraints)
    return std::make_shared<z3::expr>(ctx_.bool_val(true));
}

std::shared_ptr<z3::expr> R2EGroundedEncoder::encode_precondition_constraints(int t) {
    z3::expr_vector constraints(ctx_);
    
    // Equation (1): a^t_i → Pre^t_ai σ^t_prev(i)
    for (size_t i = 0; i < global_action_order_.size(); ++i) {
        const Action* action = global_action_order_[i]; // get the action
        int action_index = i + 1;  // Convert to 1-based indexing
        
        if (!action->has_precondition()) continue;  // No precondition to encode
        
        z3::expr action_var = variable_factory_.get_action_variable(*action, t);
        
        // Convert precondition to Z3
        auto precond_z3 = convert_expression_to_z3(action->precondition(), t);
        assert(precond_z3 && "Failed to convert precondition to Z3");
        
        // Get substitution
        auto prev_substitution = create_prev_substitution(action_index, t);
        
        // Apply substitution σ^t_prev(i)
        z3::expr substituted_precond = apply_substitution(*precond_z3, prev_substitution, t);
        
        // DEBUG: Print precondition constraints for actions 286-289 at timestep 1
        //if (t == 1 && action_index >= 286 && action_index <= 289) {
        //    std::cout << "\n=== PRECONDITION DEBUG for action " << action_index << " (" << action->name() << ") ===\n";
        //    std::cout << "Original precondition: " << precond_z3->to_string() << "\n";
        //    std::cout << "Substituted precondition: " << substituted_precond.to_string() << "\n";
        //    std::cout << "Action variable: " << action_var.to_string() << "\n";
        //    std::cout << "Constraint: " << action_var.to_string() << " → " << substituted_precond.to_string() << "\n";
        //}
        
        // a^t_i → Pre^t_ai σ^t_prev(i)
        z3::expr constraint = z3::implies(action_var, substituted_precond);
        constraints.push_back(constraint);
    }
    
    if (constraints.empty()) return std::make_shared<z3::expr>(ctx_.bool_val(true));
    return std::make_shared<z3::expr>(z3::mk_and(constraints));
}

std::shared_ptr<z3::expr> R2EGroundedEncoder::encode_effect_constraints(int t) {
    z3::expr_vector constraints(ctx_);
    
    // Equation (2): a^t_i → Eff^t_ai σ^t_modi σ^t_prev(i)
    for (size_t i = 0; i < global_action_order_.size(); ++i) {
        const Action* action = global_action_order_[i];
        int action_index = i + 1;  // Convert to 1-based indexing
        
        if (action->effects().empty()) continue;  // No effects to encode
        
        z3::expr action_var = variable_factory_.get_action_variable(*action, t);
        z3::expr_vector effect_constraints(ctx_);
        
        // Create substitutions once per action
        auto prev_substitution = create_prev_substitution(action_index, t);
        auto modi_substitution = create_modi_substitution(action_index, t);
        
        // Process each effect with both execution and carry-forward logic
        for (const Effect& effect : action->effects()) {
            z3::expr effect_constraint = encode_single_effect_with_carry_forward(
                effect,
                prev_substitution,
                modi_substitution,
                t,
                action_index,
                action_var);
            effect_constraints.push_back(effect_constraint);
        }
        
        // Add all effect constraints directly (no implication needed since it's handled in the constraint)
        for (const auto& constraint : effect_constraints) {
            constraints.push_back(constraint);
        }
    }
    
    return constraints.empty() ? 
        std::make_shared<z3::expr>(ctx_.bool_val(true)) :
        std::make_shared<z3::expr>(z3::mk_and(constraints));
}

z3::expr R2EGroundedEncoder::encode_single_effect_with_carry_forward(const Effect& effect, 
                                                                   const std::unordered_map<Expression, z3::expr>& prev_substitution,
                                                                   const std::unordered_map<Expression, z3::expr>& modi_substitution, 
                                                                   int timestep, int action_index, const z3::expr& action_var) {
    const EffectExpression& eff_expr = effect.effect_expression();
    const Expression& fluent = eff_expr.fluent();
    
    // Get chain variable (LHS - what gets assigned to) and previous value
    z3::expr chain_var = modi_substitution.at(fluent);
    z3::expr prev_value = get_prev_variable_or_chain(fluent, timestep, action_index);
    // New value when effect executes (RHS)
    z3::expr new_value = create_effect_value_z3(eff_expr, prev_value, prev_substitution, timestep);
    
    // Handle conditional vs unconditional effects
    z3::expr executed_value = prev_value;  // Initialize with default
    if (effect.is_conditional()) {
        auto condition_z3 = convert_expression_to_z3(effect.condition(), timestep);
        z3::expr substituted_condition = apply_substitution(*condition_z3, prev_substitution, timestep);
        executed_value = z3::ite(substituted_condition, new_value, prev_value);
    } else {
        executed_value = new_value; // For unconditional effects: executed_value = new_value
    }
    
    // DEBUG: Print effect constraints for actions 286-289 at timestep 1
    //if (timestep == 1 && action_index >= 286 && action_index <= 289 && 
    //    fluent.to_string().find("located plane2 city4") != std::string::npos) {
    //    std::cout << "\n=== EFFECT DEBUG for action " << action_index << " fluent " << fluent.to_string() << " ===\n";
    //    std::cout << "Chain variable: " << chain_var.to_string() << "\n";
    //    std::cout << "Previous value: " << prev_value.to_string() << "\n";
    //    std::cout << "New value: " << new_value.to_string() << "\n";
    //    std::cout << "Executed value: " << executed_value.to_string() << "\n";
    //    std::cout << "Final constraint: " << chain_var.to_string() << " == " 
    //              << "ite(" << action_var.to_string() << ", " << executed_value.to_string() 
    //              << ", " << prev_value.to_string() << ")\n";
    //}
    
    // Combined constraint: chain_var = (action_executed ? executed_value : prev_value)
    // This handles both effect execution (Equation 2) and carry-forward (Equation 3)
    return (chain_var == z3::ite(action_var, executed_value, prev_value));
}

std::shared_ptr<z3::expr> R2EGroundedEncoder::encode_linking_constraints(int t) {
    z3::expr_vector constraints(ctx_);
    
    // Equation (4): x^t = x^t_0 and x^{t+1} = x^t_ρx(|Ax|)
    for (const auto& [variable, modifying_actions] : variable_modifiers_) {
        // x^{t+1} = x^t_ρx(|Ax|) where ρx(|Ax|) is the last action that modifies x
        auto var_t_plus_1 = convert_expression_to_z3(variable, t + 1);
        
        // The final chain variable corresponds to the last modifying action
        const Action* last_action = modifying_actions.back();
        // get its index
        int final_action_idx = get_global_action_index(last_action);
        // and get the corresponding chain variable with it
        z3::expr final_chain = get_chain_variable(variable, t, final_action_idx);
        
        z3::expr linking_constraint = (*var_t_plus_1 == final_chain);
        //std::cout << "Linking constraint " << linking_constraint << " at timestep " << t << std::endl;
        constraints.push_back(linking_constraint);
    }
    
    // For variables not modified by any action, add x^{t+1} = x^t (frame axioms)
    for (const Expression& variable : all_state_variables_) {
        if (!variable_modifiers_.contains(variable)) {
            // This variable is never modified by any action, so it maintains its value
            auto var_t = convert_expression_to_z3(variable, t);
            auto var_t_plus_1 = convert_expression_to_z3(variable, t + 1);
            //std::cout << "Linking constraint " << (*var_t_plus_1 == *var_t) << " at timestep " << t << std::endl;
            constraints.push_back(*var_t_plus_1 == *var_t);
        }
    }
    
    if (constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    return std::make_shared<z3::expr>(z3::mk_and(constraints));
}

std::unordered_map<Expression, z3::expr> R2EGroundedEncoder::create_prev_substitution(int action_index, int timestep) {
    std::unordered_map<Expression, z3::expr> substitution;
    
    // First, handle variables that are modified by some action
    for (const auto& [variable, _] : variable_modifiers_) {
        z3::expr prev_var = get_prev_variable_or_chain(variable, timestep, action_index);
        substitution.emplace(variable, std::move(prev_var));
    }
    
    return substitution;
}

std::unordered_map<Expression, z3::expr> R2EGroundedEncoder::create_modi_substitution(int action_index, int timestep) {
    std::unordered_map<Expression, z3::expr> substitution;
    
    // Map each variable to its chain variable for this action
    for (const auto& [variable, _] : variable_modifiers_) {
        z3::expr chain_var = get_chain_variable(variable, timestep, action_index);
        substitution.emplace(variable, std::move(chain_var));
    }
    return substitution;
}

z3::expr R2EGroundedEncoder::apply_substitution(const z3::expr& expr, 
                                               const std::unordered_map<Expression, z3::expr>& substitution, int timestep) {
    if (substitution.empty()) return expr;
    
    // Build Z3 substitution vectors
    z3::expr_vector from(ctx_);
    z3::expr_vector to(ctx_);
    
    for (const auto& [expression, replacement] : substitution) {
        // Convert expression to Z3 with the detected timestep
        auto expr_z3 = convert_expression_to_z3(expression, timestep);
        if (expr_z3) {
            from.push_back(*expr_z3);
            to.push_back(replacement);
        }
    }
    if (from.empty()) return expr;
    
    // Apply Z3 substitution - need to cast away const
    z3::expr mutable_expr = expr;
    z3::expr result = mutable_expr.substitute(from, to);
    
    return result;
}

std::optional<z3::expr> R2EGroundedEncoder::convert_expression_to_z3_template(const Expression& expr) {
    // Convert expression without timestep for template matching
    grounded_visitor_.clear(); // start with a fresh visitor state
    grounded_visitor_.clear_timestep(); // Don't add timesteps for template variables
    accept_visitor(expr, grounded_visitor_);
    return grounded_visitor_.get_result();
}

z3::expr R2EGroundedEncoder::create_effect_value_z3(const EffectExpression& eff_expr, const z3::expr& fluent_z3,
                                                   const std::unordered_map<Expression, z3::expr>& prev_substitution, int timestep) {
    // Convert effect value with proper substitution
    auto value_z3 = convert_expression_to_z3(eff_expr.value(), timestep);
    assert(value_z3 && "Failed to convert effect value to Z3");
    z3::expr substituted_value = apply_substitution(*value_z3, prev_substitution, timestep);
    
    // Create the new value expression based on effect type
    switch (eff_expr.kind()) {
        case EffectExpression::Kind::ASSIGN:
            return substituted_value;
            
        case EffectExpression::Kind::INCREASE: {
            // Get the previous value for this fluent
            const Expression& fluent = eff_expr.fluent();
            z3::expr prev_value = (prev_substitution.contains(fluent)) ? 
                                prev_substitution.at(fluent) : fluent_z3;
            return prev_value + substituted_value;
        }
        
        case EffectExpression::Kind::DECREASE: {
            // Get the previous value for this fluent
            const Expression& fluent = eff_expr.fluent();
            z3::expr prev_value = (prev_substitution.contains(fluent)) ? 
                                prev_substitution.at(fluent) : fluent_z3;
            return prev_value - substituted_value;
        }
    }
    
    // Should never reach here
    return substituted_value;
}

std::string R2EGroundedEncoder::get_chain_variable_name(const Expression& variable, int timestep, int action_index) const {
    return variable.to_string() + "_chain_t" + std::to_string(timestep) + "_action" + std::to_string(action_index);
}

z3::expr R2EGroundedEncoder::get_chain_variable(const Expression& variable, int timestep, int action_index) {
    std::string chain_name = get_chain_variable_name(variable, timestep, action_index);
    return variable_factory_.create_symbol_variable(chain_name, variable.type());
}

z3::expr R2EGroundedEncoder::get_prev_variable_or_chain(const Expression& variable, int timestep, int action_index) {
    // Check if this variable has prev mappings
    auto prev_it = prev_x_.find(variable);
    if (prev_it == prev_x_.end() || action_index >= prev_it->second.size()) {
        // No prev mapping - use original variable
        auto var_t = convert_expression_to_z3(variable, timestep);
        assert(var_t && "Failed to convert variable to Z3");
        return *var_t;
    }
    
    int prev_index = prev_it->second[action_index];
    if (prev_index == 0) {
        // No previous modifier - use original variable x^t
        auto var_t = convert_expression_to_z3(variable, timestep);
        assert(var_t && "Failed to convert variable to Z3");
        return *var_t;
    } else {
        // Use chain variable from previous action
        return get_chain_variable(variable, timestep, prev_index);
    }
}

int R2EGroundedEncoder::get_global_action_index(const Action* action) const {
    auto it = std::find(global_action_order_.begin(), global_action_order_.end(), action);
    assert(it != global_action_order_.end() && "Action not found in global ordering");
    return std::distance(global_action_order_.begin(), it) + 1;  // +1 for 1-based indexing
}

Plan R2EGroundedEncoder::extract_plan(const z3::model& model, int max_timestep) const {
    Plan plan;
    const auto& config = Config::instance();

    if (config.is_debug()) {
        std::cout << "Extracting R2E plan from Z3 model with " << model.size() << " variable assignments" << std::endl;
    }
    
    // Double loop: first timesteps, then global action order
    // Note: action variables are not asserted for the last timestep, so stop at max_timestep - 1
    for (int t = 0; t < max_timestep; ++t) {
        // Go through actions in their global ordering
        for (const Action* action : global_action_order_) {
            z3::expr action_var = variable_factory_.get_action_variable(*action, t);
            z3::expr action_value = model.eval(action_var, true); // Use model completion
            
            if (action_value.is_true()) {
                plan.add_action(action);
                if (config.is_debug()) {
                    std::cout << "  Timestep " << t << ": " << action->name() << std::endl;
                }
            }
        }
    }

    if (config.is_debug()) {
        std::cout << "Extracted R2E plan with " << plan.length() << " actions" << std::endl;
    }
    
    return plan;
}


void R2EGroundedEncoder::debug_print_structures() const {
    std::cout << "\n========== R2E ENCODER DEBUG INFO ==========\n";
    
    // Print action ordering
    std::cout << "\n1. GLOBAL ACTION ORDERING (L):\n";
    std::cout << "   Total actions: " << global_action_order_.size() << "\n";
    for (size_t i = 0; i < global_action_order_.size(); ++i) {
        std::cout << "   [" << (i + 1) << "] " << global_action_order_[i]->name() << "\n";
    }
    
    // Print variable modifiers
    std::cout << "\n2. VARIABLE MODIFIERS (Ax sets):\n";
    std::cout << "   Modified variables: " << variable_modifiers_.size() << "\n";
    for (const auto& [variable, actions] : variable_modifiers_) {
        std::cout << "   " << variable.to_string() << " modified by " << actions.size() << " actions: ";
        for (size_t i = 0; i < actions.size(); ++i) {
            std::cout << actions[i]->name();
            if (i < actions.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }
    
    // Print rho mappings - focus on plane2 city4 issue
    std::cout << "\n3. RHO MAPPINGS (ρx) - DEBUG FOR PLANE2 ISSUE:\n";
    for (const auto& [variable, rho_values] : rho_x_) {
        if (variable.to_string().find("located plane2 city4") != std::string::npos) {
            std::cout << "   " << variable.to_string() << " → [";
            for (size_t i = 0; i < rho_values.size(); ++i) {
                std::cout << rho_values[i];
                if (i < rho_values.size() - 1) std::cout << ", ";
            }
            std::cout << "]\n";
            
            // Show which actions these correspond to
            for (size_t i = 1; i < rho_values.size(); ++i) {
                int global_idx = rho_values[i];
                if (global_idx > 0 && global_idx <= global_action_order_.size()) {
                    std::cout << "     rho[" << i << "] = " << global_idx << " (" 
                             << global_action_order_[global_idx-1]->name() << ")\n";
                }
            }
        }
    }
    
    // Print prev mappings - focus on plane2 city4 issue
    std::cout << "\n4. PREV MAPPINGS (prevx) - DEBUG FOR PLANE2 ISSUE:\n";
    for (const auto& [variable, prev_values] : prev_x_) {
        if (variable.to_string().find("located plane2 city4") != std::string::npos) {
            std::cout << "   " << variable.to_string() << " prev indices: [";
            for (size_t i = 0; i < prev_values.size(); ++i) {
                std::cout << prev_values[i];
                if (i < prev_values.size() - 1) std::cout << ", ";
            }
            std::cout << "]\n";
            
            // Show details for actions 287 and 289
            std::cout << "     Action 287 (fly-slow_plane2_city4_city1) prev: " << prev_values[287] << "\n";
            std::cout << "     Action 289 (fly-slow_plane2_city4_city3) prev: " << prev_values[289] << "\n";
        }
    }
    
    std::cout << "\n==========================================\n\n";
}

} // namespace rantanplan