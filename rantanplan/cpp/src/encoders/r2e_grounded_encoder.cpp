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
    if (Config::instance().is_debug()) {
        debug_print_structures();
    }
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

    return arpg_ordered_actions;
}


void R2EGroundedEncoder::collect_all_state_variables() {
    all_state_variables_.clear();
    for (ExprID eid : problem_.grounded_fluents()) {
        all_state_variables_.insert(eid);
    }
}

void R2EGroundedEncoder::build_variable_modifiers() {
    variable_modifiers_.clear();

    // Collect all variables and their modifying actions IN GLOBAL ORDER
    for (const Action* action : global_action_order_) {
        for (const Effect& effect : action->effects()) {
            variable_modifiers_[effect.effect_expression().fluent_id()].push_back(action);
        }
    }

    // Remove duplicates while preserving order
    for (auto& [eid, actions] : variable_modifiers_) {
        actions.erase(std::unique(actions.begin(), actions.end()), actions.end());
    }

    Stats::instance().set("encoder.r2e.modified_variables", variable_modifiers_.size());
}

void R2EGroundedEncoder::build_rho_mappings() {
    rho_x_.clear();

    for (const auto& [var_eid, modifying_actions] : variable_modifiers_) {
        std::vector<int>& rho = rho_x_[var_eid];
        rho.resize(modifying_actions.size() + 1);

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

    for (const auto& [var_eid, modifying_actions] : variable_modifiers_) {
        std::vector<int>& prev = prev_x_[var_eid];
        prev.resize(global_action_order_.size() + 1);

        for (size_t i = 0; i < global_action_order_.size(); ++i) {
            int action_index = i + 1;

            // Find the last action before current_action that modifies this variable
            const std::vector<int>& rho = rho_x_[var_eid];
            int prev_index = 0;

            for (size_t j = 1; j < rho.size(); ++j) {
                if (rho[j] < action_index) {
                    prev_index = rho[j];
                } else {
                    break;
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

    std::shared_ptr<z3::expr> effects = encode_effect_constraints(t);
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
        const Action* action = global_action_order_[i];
        int action_index = i + 1;

        if (!action->has_precondition()) continue;

        z3::expr action_var = variable_factory_.get_action_variable(*action, t);

        // Convert precondition to Z3
        z3::expr precond_z3 = convert_expr_id_to_z3(action->precondition_id(), t);

        // Get substitution
        auto prev_substitution = create_prev_substitution(action_index, t);

        // Apply substitution σ^t_prev(i)
        z3::expr substituted_precond = apply_substitution(precond_z3, prev_substitution, t);

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
    // Effects are grouped by fluent_id to produce ONE chain equation per (action, fluent) pair.
    // This correctly handles actions with multiple effects on the same fluent (e.g., one
    // unconditional and one conditional from forall/when expansion).
    for (size_t i = 0; i < global_action_order_.size(); ++i) {
        const Action* action = global_action_order_[i];
        int action_index = i + 1;

        if (action->effects().empty()) continue;

        z3::expr action_var = variable_factory_.get_action_variable(*action, t);

        // Create substitutions once per action
        auto prev_substitution = create_prev_substitution(action_index, t);
        auto modi_substitution = create_modi_substitution(action_index, t);

        // Group effects by fluent_id
        std::unordered_map<ExprID, std::vector<const Effect*>> effects_by_fluent;
        for (const Effect& effect : action->effects()) {
            effects_by_fluent[effect.effect_expression().fluent_id()].push_back(&effect);
        }

        // Create one chain equation per modified fluent
        for (const auto& [fluent_id, effects] : effects_by_fluent) {
            z3::expr chain_var = modi_substitution.at(fluent_id);
            z3::expr prev_value = get_prev_variable_or_chain(fluent_id, t, action_index);

            // Combine all effects on this fluent into a single executed_value.
            //
            // For ASSIGN (boolean or numeric):
            //   - Unconditional: last one wins (they should agree; multiple
            //     conflicting ASSIGNs is ill-formed PDDL).
            //   - Conditional: nested ITEs selecting one winner.
            //
            // For INCREASE/DECREASE (numeric):
            //   - Multiple effects must ACCUMULATE (sum of deltas), not
            //     override each other.  This arises from forall/when expansion
            //     producing multiple additive effects on the same fluent.
            //   - Unconditional: chain through create_effect_value_z3 which
            //     computes prev+delta, feeding each result as prev to the next.
            //   - Conditional: add ite(cond, delta, 0) per effect.

            // Partition into unconditional and conditional.
            std::vector<const Effect*> unconditional, conditional;
            for (const Effect* eff : effects) {
                if (eff->is_conditional()) conditional.push_back(eff);
                else unconditional.push_back(eff);
            }

            z3::expr executed_value = prev_value;

            if (!unconditional.empty()) {
                // Accumulate all unconditional effects. For ASSIGN the last
                // one wins (single pass is fine). For INCREASE/DECREASE each
                // call adds its delta onto the running value.
                for (const Effect* eff : unconditional) {
                    executed_value = create_effect_value_z3(
                        eff->effect_expression(), executed_value, prev_substitution, t);
                }
            }

            if (!conditional.empty()) {
                // Conditional effects: behaviour depends on effect kind.
                for (const Effect* eff : conditional) {
                    z3::expr condition_z3 = convert_expr_id_to_z3(
                        eff->effect_expression().condition_id(), t);
                    z3::expr substituted_condition = apply_substitution(
                        condition_z3, prev_substitution, t);

                    if (eff->effect_expression().is_assign()) {
                        // ASSIGN: ITE selects this value or keeps the running value.
                        z3::expr new_value = create_effect_value_z3(
                            eff->effect_expression(), executed_value, prev_substitution, t);
                        executed_value = z3::ite(substituted_condition, new_value, executed_value);
                    } else {
                        // INCREASE/DECREASE: accumulate guarded delta.
                        // create_effect_value_z3 returns (executed_value ± delta),
                        // so ite(cond, that, executed_value) adds delta only when cond holds.
                        z3::expr with_delta = create_effect_value_z3(
                            eff->effect_expression(), executed_value, prev_substitution, t);
                        executed_value = z3::ite(substituted_condition, with_delta, executed_value);
                    }
                }
            }

            constraints.push_back(chain_var == z3::ite(action_var, executed_value, prev_value));
        }
    }

    return constraints.empty() ?
        std::make_shared<z3::expr>(ctx_.bool_val(true)) :
        std::make_shared<z3::expr>(z3::mk_and(constraints));
}

std::shared_ptr<z3::expr> R2EGroundedEncoder::encode_linking_constraints(int t) {
    z3::expr_vector constraints(ctx_);

    // Equation (4): x^{t+1} = x^t_ρx(|Ax|)
    for (const auto& [var_eid, modifying_actions] : variable_modifiers_) {
        z3::expr var_t_plus_1 = convert_expr_id_to_z3(var_eid, t + 1);

        // The final chain variable corresponds to the last modifying action
        const Action* last_action = modifying_actions.back();
        // get its index
        int final_action_idx = get_global_action_index(last_action);
        // and get the corresponding chain variable with it
        z3::expr final_chain = get_chain_variable(var_eid, t, final_action_idx);

        z3::expr linking_constraint = (var_t_plus_1 == final_chain);
        constraints.push_back(linking_constraint);
    }

    // For variables not modified by any action, add x^{t+1} = x^t
    for (ExprID eid : problem_.grounded_fluents()) {
        if (variable_modifiers_.find(eid) == variable_modifiers_.end()) {
            z3::expr var_t = convert_expr_id_to_z3(eid, t);
            z3::expr var_t_plus_1 = convert_expr_id_to_z3(eid, t + 1);
            constraints.push_back(var_t_plus_1 == var_t);
        }
    }

    if (constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    return std::make_shared<z3::expr>(z3::mk_and(constraints));
}

std::unordered_map<ExprID, z3::expr> R2EGroundedEncoder::create_prev_substitution(int action_index, int timestep) {
    std::unordered_map<ExprID, z3::expr> substitution;

    for (const auto& [var_eid, _] : variable_modifiers_) {
        z3::expr prev_var = get_prev_variable_or_chain(var_eid, timestep, action_index);
        substitution.emplace(var_eid, std::move(prev_var));
    }

    return substitution;
}

std::unordered_map<ExprID, z3::expr> R2EGroundedEncoder::create_modi_substitution(int action_index, int timestep) {
    std::unordered_map<ExprID, z3::expr> substitution;

    for (const auto& [var_eid, _] : variable_modifiers_) {
        z3::expr chain_var = get_chain_variable(var_eid, timestep, action_index);
        substitution.emplace(var_eid, std::move(chain_var));
    }
    return substitution;
}

z3::expr R2EGroundedEncoder::apply_substitution(const z3::expr& expr,
                                               const std::unordered_map<ExprID, z3::expr>& substitution, int timestep) {
    if (substitution.empty()) return expr;

    z3::expr_vector from(ctx_);
    z3::expr_vector to(ctx_);

    for (const auto& [var_eid, replacement] : substitution) {
        z3::expr expr_z3 = convert_expr_id_to_z3(var_eid, timestep);
        from.push_back(expr_z3);
        to.push_back(replacement);
    }
    if (from.empty()) return expr;

    z3::expr mutable_expr = expr;
    return mutable_expr.substitute(from, to);
}

z3::expr R2EGroundedEncoder::create_effect_value_z3(const EffectExpression& eff_expr, const z3::expr& fluent_z3,
                                                   const std::unordered_map<ExprID, z3::expr>& prev_substitution, int timestep) {
    // Convert the delta/value expression and substitute any fluent references
    // in it (e.g., "increase (fuel ?v) (distance ?from ?to)" — distance is a
    // fluent reference inside the delta that must be resolved via prev_substitution).
    z3::expr value_z3 = convert_expr_id_to_z3(eff_expr.value_id(), timestep);
    z3::expr substituted_value = apply_substitution(value_z3, prev_substitution, timestep);

    switch (eff_expr.kind()) {
        case EffectExpression::Kind::ASSIGN:
            return substituted_value;

        case EffectExpression::Kind::INCREASE:
            // fluent_z3 is the running accumulated value passed by the caller.
            // Using it (not prev_substitution[fluent]) ensures multiple
            // INCREASE effects on the same fluent sum their deltas correctly.
            return fluent_z3 + substituted_value;

        case EffectExpression::Kind::DECREASE:
            return fluent_z3 - substituted_value;
    }

    // Should never reach here
    return substituted_value;
}

std::string R2EGroundedEncoder::get_chain_variable_name(ExprID var_eid, int timestep, int action_index) const {
    return problem_.pool().to_string(var_eid) + "_chain_t" + std::to_string(timestep) + "_action" + std::to_string(action_index);
}

z3::expr R2EGroundedEncoder::get_chain_variable(ExprID var_eid, int timestep, int action_index) {
    std::string chain_name = get_chain_variable_name(var_eid, timestep, action_index);
    return variable_factory_.create_symbol_variable(chain_name, problem_.type_for_id(var_eid));
}

z3::expr R2EGroundedEncoder::get_prev_variable_or_chain(ExprID var_eid, int timestep, int action_index) {
    // Check if this variable has prev mappings
    auto prev_it = prev_x_.find(var_eid);
    if (prev_it == prev_x_.end() || action_index >= static_cast<int>(prev_it->second.size())) {
        // No prev mapping - use original variable
        return convert_expr_id_to_z3(var_eid, timestep);
    }

    int prev_index = prev_it->second[action_index];
    if (prev_index == 0) {
        // No previous modifier - use original variable x^t
        return convert_expr_id_to_z3(var_eid, timestep);
    } else {
        // Use chain variable from previous action
        return get_chain_variable(var_eid, timestep, prev_index);
    }
}

int R2EGroundedEncoder::get_global_action_index(const Action* action) const {
    auto it = std::find(global_action_order_.begin(), global_action_order_.end(), action);
    assert(it != global_action_order_.end() && "Action not found in global ordering");
    return std::distance(global_action_order_.begin(), it) + 1;
}

z3::expr R2EGroundedEncoder::convert_cost_to_z3(const Action& action, int timestep) {
    // R2E's fixed ordering means action a_i sees state σ^t_{prev(i)},
    // not x^t. Apply the same prev_substitution used for preconditions
    // so that SDAC cost expressions reference the correct intermediate state.
    z3::expr cost_z3 = convert_expr_id_to_z3(action.cost_id(), timestep);
    int action_index = get_global_action_index(&action);
    auto prev_substitution = create_prev_substitution(action_index, timestep);
    return apply_substitution(cost_z3, prev_substitution, timestep);
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
            z3::expr action_value = model.eval(action_var, true);

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

    std::cout << "\n1. GLOBAL ACTION ORDERING (L):\n";
    std::cout << "   Total actions: " << global_action_order_.size() << "\n";
    for (size_t i = 0; i < global_action_order_.size(); ++i) {
        std::cout << "   [" << (i + 1) << "] " << global_action_order_[i]->name() << "\n";
    }

    std::cout << "\n2. VARIABLE MODIFIERS (Ax sets):\n";
    std::cout << "   Modified variables: " << variable_modifiers_.size() << "\n";
    for (const auto& [var_eid, actions] : variable_modifiers_) {
        std::cout << "   " << problem_.pool().to_string(var_eid) << " modified by " << actions.size() << " actions: ";
        for (size_t i = 0; i < actions.size(); ++i) {
            std::cout << actions[i]->name();
            if (i < actions.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }

    std::cout << "\n3. RHO MAPPINGS (ρx):\n";
    for (const auto& [var_eid, rho_values] : rho_x_) {
        std::cout << "   " << problem_.pool().to_string(var_eid) << " → [";
        for (size_t i = 0; i < rho_values.size(); ++i) {
            std::cout << rho_values[i];
            if (i < rho_values.size() - 1) std::cout << ", ";
        }
        std::cout << "]\n";

        for (size_t i = 1; i < rho_values.size(); ++i) {
            int global_idx = rho_values[i];
            if (global_idx > 0 && static_cast<size_t>(global_idx) <= global_action_order_.size()) {
                std::cout << "     rho[" << i << "] = " << global_idx << " ("
                         << global_action_order_[global_idx-1]->name() << ")\n";
            }
        }
    }

    std::cout << "\n4. PREV MAPPINGS (prevx):\n";
    for (const auto& [var_eid, prev_values] : prev_x_) {
        std::cout << "   " << problem_.pool().to_string(var_eid) << " prev indices: [";
        for (size_t i = 0; i < prev_values.size(); ++i) {
            std::cout << prev_values[i];
            if (i < prev_values.size() - 1) std::cout << ", ";
        }
        std::cout << "]\n";
    }

    std::cout << "\n==========================================\n\n";
}

} // namespace rantanplan
