#include "chained_grounded_encoder.hpp"
#include "../util/stats.hpp"
#include <iostream>
#include <algorithm>
#include <cassert>

namespace rantanplan {

ChainedGroundedEncoder::ChainedGroundedEncoder(const Problem& problem, z3::context& ctx)
    : GroundedEncoder(problem, ctx) {
    build_variable_modifiers_index();
}

void ChainedGroundedEncoder::build_variable_modifiers_index() {
    variable_modifiers_.clear();

    // Collect all variables and their modifying actions
    for (const Action& action : problem_.actions()) {
        for (const Effect& effect : action.effects()) {
            variable_modifiers_[effect.effect_expression().fluent_id()].push_back(&action);
        }
    }

    // Remove duplicates from modifier lists
    for (auto& [eid, actions] : variable_modifiers_) {
        std::sort(actions.begin(), actions.end());
        actions.erase(std::unique(actions.begin(), actions.end()), actions.end());
    }

    auto& stats = Stats::instance();
    size_t chained_vars = 0;
    for (const auto& [eid, actions] : variable_modifiers_) {
        if (actions.size() > 1 && !problem_.is_bool_type(eid)) {
            chained_vars++;
        }
    }
    stats.set("encoder.chained_variables", chained_vars);
}


std::shared_ptr<z3::expr> ChainedGroundedEncoder::encode_actions(int t) {
    z3::expr_vector all_constraints(ctx_);
    auto& stats = Stats::instance();

    // Process each action with modified effect constraints
    for (const Action& action : problem_.actions()) {
        z3::expr action_var = variable_factory_.get_action_variable(action, t);

        if (action.effects().empty()) continue;

        // Create precondition constraints (unchanged)
        if (action.has_precondition()) {
            auto z3_precond = convert_expr_id_to_z3(action.precondition_id(), t);
            all_constraints.push_back(z3::implies(action_var, *z3_precond));
        }

        // Create effect constraints, filtering out multi-modified non-Boolean variables
        z3::expr_vector filtered_effects(ctx_);

        for (const Effect& effect : action.effects()) {
            ExprID var_eid = effect.effect_expression().fluent_id();

            // Check if this variable needs chained encoding
            auto mod_it = variable_modifiers_.find(var_eid);
            bool needs_chaining = (mod_it != variable_modifiers_.end() &&
                                 mod_it->second.size() > 1 &&
                                 !problem_.is_bool_type(var_eid));

            if (!needs_chaining) {
                // Use standard effect constraint for Boolean vars or single-modified vars
                auto z3_effect = convert_effect_to_z3(effect.effect_expression(), t);
                if (z3_effect) {
                    filtered_effects.push_back(*z3_effect);
                }
            }
            // Multi-modified non-Boolean variables are handled by chained encoding
        }

        // Add filtered effect constraints for this action
        if (!filtered_effects.empty()) {
            all_constraints.push_back(z3::implies(action_var, z3::mk_and(filtered_effects)));
        }
    }

    // Add chained effect constraints for multi-modified non-Boolean variables
    auto chained_constraints = encode_chained_effects(t);
    if (chained_constraints) {
        all_constraints.push_back(*chained_constraints);
    }

    stats.add("encoder.action_constraints", all_constraints.size());

    if (all_constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }

    return std::make_shared<z3::expr>(z3::mk_and(all_constraints));
}

std::shared_ptr<z3::expr> ChainedGroundedEncoder::encode_chained_effects(int t) {
    z3::expr_vector constraints(ctx_);
    auto& stats = Stats::instance();

    // Implement Section 5.2 chained encoding for multi-modified variables
    // Creates chain variables ζ^t_{n,i} to handle cumulative effects from parallel actions
    // Key insight: conditions are evaluated using current chain position (input),
    // while effects produce the next chain position (output)
    for (const auto& [var_eid, modifying_actions] : variable_modifiers_) {
        // Skip variables that don't need chaining
        if (modifying_actions.size() <= 1 || problem_.is_bool_type(var_eid)) {
            continue;
        }

        const std::string var_str = problem_.pool().to_string(var_eid);
        const Type* var_type = problem_.type_for_id(var_eid);

        // Create intermediate chain variables ζ^t_{n,i}
        std::vector<z3::expr> chain_vars;

        // Equation (7): n^t = ζ^t_{n,0}
        auto var_t = convert_expr_id_to_z3(var_eid, t);
        assert(var_t && "Failed to convert variable to Z3");
        chain_vars.push_back(*var_t);

        // Create chain variables and constraints for each modifying action
        for (size_t i = 0; i < modifying_actions.size(); ++i) {
            const Action* action = modifying_actions[i];
            z3::expr action_var = variable_factory_.get_action_variable(*action, t);

            // Create next chain variable ζ^t_{n,i+1}
            std::string chain_var_name = "zeta_" + var_str + "_t" +
                                       std::to_string(t) + "_i" + std::to_string(i + 1);
            z3::expr chain_var_next = variable_factory_.create_symbol_variable(chain_var_name, var_type);
            chain_vars.push_back(chain_var_next);

            // Get the effect for this variable from this action
            auto effect_opt = get_effect_for_variable(*action, var_eid);
            assert(effect_opt && "Action expected to modify variable but no effect found?");

            // Equation (8): Apply effect conditionally based on action execution
            z3::expr chain_prev = chain_vars[i];
            z3::expr effect_applied = apply_effect_to_expression(**effect_opt, chain_prev, t);

            // Handle conditional effects by finding the full Effect object
            const Effect* full_effect = nullptr;
            for (const Effect& eff : action->effects()) {
                if (eff.effect_expression().fluent_id() == var_eid) {
                    full_effect = &eff;
                    break;
                }
            }

            z3::expr chain_constraint = ctx_.bool_val(true);
            if (full_effect && full_effect->is_conditional()) {
                // For conditional effects: (action^t ∧ condition^t) → ζ^t_{n,i+1} = eff(ζ^t_{n,i})
                //                          ¬(action^t ∧ condition^t) → ζ^t_{n,i+1} = ζ^t_{n,i}
                // This ensures chain variable passes through unchanged when condition is false
                auto condition_z3 = convert_expr_id_to_z3(full_effect->effect_expression().condition_id(), t);
                z3::expr apply_effect = action_var && *condition_z3;
                chain_constraint = z3::ite(apply_effect,
                                         chain_var_next == effect_applied,
                                         chain_var_next == chain_prev);
            } else {
                // For unconditional effects: action^t → ζ^t_{n,i+1} = eff(ζ^t_{n,i})
                //                            ¬action^t → ζ^t_{n,i+1} = ζ^t_{n,i}
                chain_constraint = z3::ite(action_var,
                                         chain_var_next == effect_applied,
                                         chain_var_next == chain_prev);
            }

            constraints.push_back(chain_constraint);
        }

        // Equation (9): n^{t+1} = ζ^t_{n,|A_n|}
        auto var_t_plus_1 = convert_expr_id_to_z3(var_eid, t + 1);
        assert(var_t_plus_1 && "Failed to convert variable to Z3 at next timestep");
        constraints.push_back(*var_t_plus_1 == chain_vars.back());
    }

    stats.add("encoder.chained_constraints", constraints.size());

    if (constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }

    return std::make_shared<z3::expr>(z3::mk_and(constraints));
}

std::optional<const EffectExpression*> ChainedGroundedEncoder::get_effect_for_variable(
    const Action& action, ExprID var_eid) const {

    for (const Effect& effect : action.effects()) {
        if (effect.effect_expression().fluent_id() == var_eid) {
            return &effect.effect_expression();
        }
    }
    return std::nullopt;
}

z3::expr ChainedGroundedEncoder::apply_effect_to_expression(
    const EffectExpression& effect, const z3::expr& base_expr, int timestep) const {

    // Convert the effect value to Z3 - cast away const for this method
    auto* non_const_this = const_cast<ChainedGroundedEncoder*>(this);
    auto value_z3 = non_const_this->convert_expr_id_to_z3(effect.value_id(), timestep);
    assert(value_z3 && "Failed to convert effect value to Z3");

    // Apply the effect operation to the base expression
    // Note: We can't reuse convert_effect_to_z3() from base class because that creates
    // a constraint (fluent_next == result), but we need the result expression itself
    switch (effect.kind()) {
        case EffectExpression::Kind::ASSIGN:
            return *value_z3;

        case EffectExpression::Kind::INCREASE:
            return base_expr + *value_z3;

        case EffectExpression::Kind::DECREASE:
            return base_expr - *value_z3;
    }

    // Fallback - should never reach here
    return base_expr;
}

} // namespace rantanplan
