#include "semantic_interference_analysis.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/logger.hpp"
#include "../config/config.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <unordered_set>

namespace rantanplan {

SemanticInterferenceAnalysis::SemanticInterferenceAnalysis(const Problem& problem) {
    initialize(problem);
}

void SemanticInterferenceAnalysis::initialize(const Problem& problem) {
    problem_ = &problem;
    
    // Clear any existing data
    action_analysis_.clear();
    semantic_cache_.clear();
    source_cache_.clear();
    
    // Setup common functionality using base class methods
    analyze_all_actions();
    
    // Initialize Z3 context immediately
    z3_context_ = std::make_unique<z3::context>();
    z3_solver_ = std::make_unique<z3::solver>(*z3_context_);
    
    // Create grounded visitor for expression conversion
    z3_variable_factory_ = std::make_unique<Z3VariableFactory>(*z3_context_);
    z3_variable_factory_->set_problem(problem_);
    grounded_visitor_ = std::make_unique<GroundedEncodingVisitor>(*z3_context_, problem_, z3_variable_factory_.get());

}

bool SemanticInterferenceAnalysis::has_interference(const Action& a1, const Action& a2) const {
    // This implements the directional "affects" relationship from Definition 3.9
    // a1 affects a2. Remember that this is NOT symmetric
    return action_affects_semantically(a1, a2);
}

bool SemanticInterferenceAnalysis::has_interference(int node_id1, int node_id2) const {
    // Convert node IDs back to actions, then check if action1 affects action2
    const Action* action1 = &problem_->action(node_id1);
    const Action* action2 = &problem_->action(node_id2);
    return action_affects_semantically(*action1, *action2);
}

bool SemanticInterferenceAnalysis::action_affects_semantically(const Action& a1, const Action& a2) const {
    bool affects = false; // by default a1 does not affect a2

    // Use action IDs as cache key (unique per ground action instance).
    // Names alone are not unique — e.g. C++ grounding produces multiple
    // ground actions all named "pick" with different parameters.
    auto cache_key = std::make_pair(a1.id(), a2.id());
    
    // Check if result is already cached
    auto cache_it = semantic_cache_.find(cache_key);
    if (cache_it != semantic_cache_.end()) return cache_it->second;
    
    // If a1 has conditional effects on the same fluent with non-exclusive
    // conditions, the ite-based substitution may not faithfully model the
    // combined effect. Conservatively report interference in that case.
    InterferenceSource source = InterferenceSource::NONE;
    if (has_conflicting_conditional_effects(a1)) {
        affects = true;
        source = InterferenceSource::CONFLICTING_COND_EFFECTS;
    } else {
        // Based on Definition 3.9 from the paper:
        // Action a affects action b if either:
        // 1. a can prevent b's execution (pre(b) ∧ σ(a, ¬pre(b)) is satisfiable)
        // 2. a's effects don't commute properly with b's effects
        if (check1(a1, a2)) {
            affects = true;
            source = InterferenceSource::CHECK1;
        } else {
            // check2 returns the specific sub-case
            InterferenceSource c2 = check2_source(a1, a2);
            if (c2 != InterferenceSource::NONE) {
                affects = true;
                source = c2;
            }
        }
    }

    // Cache the directional "affects" result and its source
    semantic_cache_[cache_key] = affects;
    source_cache_[cache_key] = source;
    return affects;
}

InterferenceSource SemanticInterferenceAnalysis::get_interference_source(int source_id, int target_id) const {
    auto it = source_cache_.find({source_id, target_id});
    if (it != source_cache_.end()) return it->second;
    return InterferenceSource::NONE;
}



z3::expr SemanticInterferenceAnalysis::apply_action_effects_substitution(const Action& action, const z3::expr& target_expr) const {
    // Apply action's effects as substitution to the target expression.
    // This implements the σ(action, expr) operation from the paper.
    //
    // Multiple conditional effects on the same fluent are composed into
    // a single nested ite expression to avoid Z3's substitute() silently
    // dropping duplicates (it only applies the first match per key).
    if (action.effects().empty()) {
        return target_expr;
    }

    // Group effects by fluent ExprID to properly compose multiple effects
    std::unordered_map<ExprID, std::vector<const Effect*>> effects_by_fluent;
    std::vector<ExprID> fluent_order; // preserve first-seen order
    for (const Effect& effect : action.effects()) {
        ExprID fid = effect.effect_expression().fluent_id();
        if (effects_by_fluent.find(fid) == effects_by_fluent.end()) {
            fluent_order.push_back(fid);
        }
        effects_by_fluent[fid].push_back(&effect);
    }

    z3::expr_vector from_vector(*z3_context_);
    z3::expr_vector to_vector(*z3_context_);

    for (ExprID fid : fluent_order) {
        z3::expr fluent_z3 = grounded_visitor_->convert_from_pool(fid, -1);
        z3::expr composed = fluent_z3; // start with unchanged fluent

        // Compose effects into a nested ite chain. Correct when conditions
        // are mutually exclusive (the common PDDL pattern). Non-exclusive
        // conditions are detected by has_conflicting_conditional_effects()
        // which conservatively reports interference for those actions.
        for (const Effect* effect : effects_by_fluent[fid]) {
            const EffectExpression& eff_expr = effect->effect_expression();
            z3::expr new_value_z3 = convert_effect_to_z3(eff_expr, fluent_z3);

            if (effect->is_conditional()) {
                z3::expr condition_z3 = grounded_visitor_->convert_from_pool(
                    effect->effect_expression().condition_id(), -1);
                composed = z3::ite(condition_z3, new_value_z3, composed);
            } else {
                composed = new_value_z3; // unconditional overrides all
            }
        }

        from_vector.push_back(fluent_z3);
        to_vector.push_back(composed);
    }

    z3::expr result = target_expr;
    return result.substitute(from_vector, to_vector);
}

bool SemanticInterferenceAnalysis::check1(const Action& a1, const Action& a2) const {
    // Condition 1 of Definition 3.9: pre(a2) ∧ σ(a1, ¬pre(a2)) is satisfiable
    // in other words: can a1 prevent a2's execution?
    if (!a2.has_precondition()) return false; // If a2 has no precondition, a1 cannot prevent its execution

    // Convert preconditions to Z3
    z3::expr a1_pre = convert_precondition_to_z3(a1);
    z3::expr a2_pre = convert_precondition_to_z3(a2);
    
    // Apply a1's effects to negated a2 precondition using substitution
    z3::expr substituted_expr = apply_action_effects_substitution(a1, a2_pre);

    // Check if substitution actually changed anything
    if (z3::eq(a2_pre, substituted_expr)) {
        // If no substitution occurred, then a1 doesn't affect a2's precondition
        return false;
    }
    
    // Check condition 1: pre(a1) ∧ pre(a2) ∧ ¬(pre(a2)σ(eff(a1)) is satisfiable
    z3::expr check1 = a1_pre && a2_pre && !substituted_expr;

    // Push context for satisfiability check
    z3_solver_->push();
    z3_solver_->add(check1);
    z3::check_result result = z3_solver_->check();
    z3_solver_->pop(); // Always pop to restore solver state
    
    return (result == z3::sat); // Source can prevent a2's execution if satisfiable
}

InterferenceSource SemanticInterferenceAnalysis::check2_source(const Action& a1, const Action& a2) const {
    // Condition 2 of Definition 3.9:
    //   2a. Not simply commuting (Def 3.5/3.3) → unconditional interference
    //   2b. Simply commuting but happening ≠ sequential → state-dependent

    if (!are_simply_commuting(a1, a2)) {
        return InterferenceSource::CHECK2_UNCOMM;
    }

    // Simply commuting — check if happening effect matches sequential execution.
    // Formula: pre(a1) ∧ pre(a2) ∧ ¬(x σ_{h({a1,a2})} = x σ_a2 ∘ σ_a1)
    z3::expr a1_pre = convert_precondition_to_z3(a1);
    z3::expr a2_pre = convert_precondition_to_z3(a2);

    std::unordered_set<ExprID> affected_var_eids;
    for (const Effect& effect : a1.effects()) {
        affected_var_eids.insert(effect.effect_expression().fluent_id());
    }
    for (const Effect& effect : a2.effects()) {
        affected_var_eids.insert(effect.effect_expression().fluent_id());
    }

    for (ExprID var_eid : affected_var_eids) {
        z3::expr var_z3 = grounded_visitor_->convert_from_pool(var_eid, -1);

        // Happening effect: compose effects in parallel
        z3::expr var_after_happening = var_z3;
        var_after_happening = apply_action_effects_substitution(a2, var_after_happening);
        var_after_happening = apply_action_effects_substitution(a1, var_after_happening);

        // Sequential effect: apply a1 then a2
        z3::expr var_after_sequential = var_z3;
        var_after_sequential = apply_action_effects_substitution(a1, var_after_sequential);
        var_after_sequential = apply_action_effects_substitution(a2, var_after_sequential);

        if (z3::eq(var_after_happening, var_after_sequential)) {
            continue;
        }

        z3::expr non_commutativity = a1_pre && a2_pre && (var_after_happening != var_after_sequential);

        z3_solver_->push();
        z3_solver_->add(non_commutativity);
        z3::check_result result = z3_solver_->check();
        z3_solver_->pop();

        if (result == z3::sat) {
            return InterferenceSource::CHECK2_HAPPEN;
        }
    }

    return InterferenceSource::NONE;
}

bool SemanticInterferenceAnalysis::are_simply_commuting(const Action& a1, const Action& a2) const {
    // Two actions are simply commuting if for every variable x modified by both,
    // the assignments {x → exp1} and {x → exp2} commute
    
    // Build maps from variables (by ExprID) to their effects for both actions.
    // If any fluent has multiple effects (e.g. paired conditional effects),
    // simple commutativity doesn't apply — fall through to the full check.
    std::unordered_map<ExprID, const EffectExpression*> a1_effects;
    std::unordered_map<ExprID, const EffectExpression*> a2_effects;

    for (const Effect& effect : a1.effects()) {
        ExprID fid = effect.effect_expression().fluent_id();
        if (a1_effects.count(fid)) return false; // multiple effects on same fluent
        a1_effects[fid] = &effect.effect_expression();
    }
    for (const Effect& effect : a2.effects()) {
        ExprID fid = effect.effect_expression().fluent_id();
        if (a2_effects.count(fid)) return false; // multiple effects on same fluent
        a2_effects[fid] = &effect.effect_expression();
    }

    // Check commutativity for each variable modified by both actions
    for (const auto& [var_eid, a1_effect] : a1_effects) {
        auto a2_it = a2_effects.find(var_eid);
        if (a2_it != a2_effects.end()) {
            auto a2_effect = a2_it->second;
            if (!assignments_commute(*a1_effect, *a2_effect, var_eid)) {
                return false;
            }
        }
    }
    
    return true; // All common variables commute
}

bool SemanticInterferenceAnalysis::assignments_commute(const EffectExpression& eff1,
                                                       const EffectExpression& eff2,
                                                       ExprID var_eid) const {
    // Check if two assignments to the same variable commute
    // According to Definition 3.3: T ⊨ (exp2{x → exp1} = exp1{x → exp2})

    z3::expr var_z3 = grounded_visitor_->convert_from_pool(var_eid, -1);
    
    // Create the proper effect expressions based on effect type
    z3::expr exp1 = convert_effect_to_z3(eff1, var_z3);
    z3::expr exp2 = convert_effect_to_z3(eff2, var_z3);
    
    // Create substitutions: exp2{x → exp1} and exp1{x → exp2}
    z3::expr_vector from_vec(*z3_context_);
    z3::expr_vector to_vec1(*z3_context_);
    z3::expr_vector to_vec2(*z3_context_);
    
    from_vec.push_back(var_z3);
    to_vec1.push_back(exp1);
    to_vec2.push_back(exp2);
    
    z3::expr exp2_substituted = exp2.substitute(from_vec, to_vec1); // exp2{x → exp1}
    z3::expr exp1_substituted = exp1.substitute(from_vec, to_vec2); // exp1{x → exp2}
    
    // Check if ¬(exp2{x → exp1} = exp1{x → exp2}) is satisfiable
    z3::expr non_commute = (exp2_substituted != exp1_substituted);
    
    z3_solver_->push();
    z3_solver_->add(non_commute);
    z3::check_result result = z3_solver_->check();
    z3_solver_->pop();
    
    return (result == z3::unsat); // They commute if non-commutativity is unsatisfiable
}


std::vector<const Action*> SemanticInterferenceAnalysis::topological_sort_actions(const std::vector<const Action*>& actions) const {
    if (actions.size() <= 1) {
        std::vector<const Action*> result;
        result.reserve(actions.size());
        for (const auto& action : actions) {
            result.push_back(action);
        }
        return result;
    }
    
    // Convert actions to their existing node IDs from the base class mappings
    std::vector<int> node_ids;
    for (const Action* action : actions) {
        int node_id = action->id();
        if (node_id >= 0) { // Valid node ID found
            node_ids.push_back(node_id);
        }
    }
    
    if (node_ids.empty()) {
        return actions; // No valid node IDs found, return as-is
    }
    
    // Build a temporary subgraph using existing node IDs and cached semantic interferences
    Graph temp_subgraph(problem_->action_count());
    
    // Add edges using cached semantic interference results
    for (size_t i = 0; i < actions.size(); ++i) {
        for (size_t j = 0; j < actions.size(); ++j) {
            if (i != j) {
                const Action* action1 = actions[i];
                const Action* action2 = actions[j];
                
                // Add directed edge if action1 affects action2
                if (has_interference(*action1, *action2)) {
                    int node1 = action1->id();
                    int node2 = action2->id();
                    if (node1 >= 0 && node2 >= 0) {
                        temp_subgraph.add_edge(node1, node2);
                    }
                }
            }
        }
    }
    
    // Use Graph's topological sort for execution order
    std::vector<int> sorted_node_ids = temp_subgraph.topological_sort(node_ids);
    
    // Convert back to actions using existing base class mappings
    std::vector<const Action*> result;
    for (int node_id : sorted_node_ids) {
        const Action* action = &problem_->action(node_id);
        if (action) {
            result.push_back(action);
        }
    }
    
    return result;
}


z3::expr SemanticInterferenceAnalysis::convert_precondition_to_z3(const Action& action) const {
    if (!action.has_precondition()) {
        return z3_context_->bool_val(true);
    }
    return grounded_visitor_->convert_from_pool(action.precondition_id(), -1);
}

z3::expr SemanticInterferenceAnalysis::convert_effect_to_z3(const EffectExpression& effect, const z3::expr& base_var_z3) const {
    switch (effect.kind()) {
        case EffectExpression::Kind::ASSIGN:
            return grounded_visitor_->convert_from_pool(effect.value_id(), -1);
        case EffectExpression::Kind::INCREASE:
            return base_var_z3 + grounded_visitor_->convert_from_pool(effect.value_id(), -1);
        case EffectExpression::Kind::DECREASE:
            return base_var_z3 - grounded_visitor_->convert_from_pool(effect.value_id(), -1);
    }
    throw std::runtime_error("Unknown effect kind");
}

// Graph-based methods (NOT supported by semantic lazy analysis)

const Graph& SemanticInterferenceAnalysis::get_interference_graph() const {
    throw std::runtime_error("SemanticInterferenceAnalysis does not support get_interference_graph(). "
                            "This method is only available with eager analysis. "
                            "Use has_interference() for individual interference checks instead.");
}

const std::vector<int>& SemanticInterferenceAnalysis::get_neighbours(int node_id) const {
    throw std::runtime_error("SemanticInterferenceAnalysis does not support get_neighbours(). "
                            "This method is only available with eager analysis. "
                            "Use has_interference() for individual interference checks instead.");
}

void SemanticInterferenceAnalysis::output_interference_graph_dot(const std::string& filename) const {
    throw std::runtime_error("SemanticInterferenceAnalysis does not support output_interference_graph_dot(). "
                            "This method is only available with eager analysis as it requires the full graph.");
}

bool SemanticInterferenceAnalysis::has_conflicting_conditional_effects(const Action& action) const {
    auto cache_it = conflicting_effects_cache_.find(action.id());
    if (cache_it != conflicting_effects_cache_.end()) return cache_it->second;

    bool has_conflict = false;

    // Group conditional effects by fluent
    std::unordered_map<ExprID, std::vector<const Effect*>> cond_by_fluent;
    for (const Effect& effect : action.effects()) {
        if (effect.is_conditional()) {
            cond_by_fluent[effect.effect_expression().fluent_id()].push_back(&effect);
        }
    }

    z3::expr pre = convert_precondition_to_z3(action);

    for (const auto& [fid, effects] : cond_by_fluent) {
        if (effects.size() < 2) continue;

        // Check all pairs of conditions for simultaneous satisfiability
        for (size_t i = 0; i < effects.size() && !has_conflict; ++i) {
            for (size_t j = i + 1; j < effects.size() && !has_conflict; ++j) {
                z3::expr ci = grounded_visitor_->convert_from_pool(
                    effects[i]->effect_expression().condition_id(), -1);
                z3::expr cj = grounded_visitor_->convert_from_pool(
                    effects[j]->effect_expression().condition_id(), -1);

                z3_solver_->push();
                z3_solver_->add(pre && ci && cj);
                has_conflict = (z3_solver_->check() == z3::sat);
                z3_solver_->pop();
            }
        }
        if (has_conflict) break;
    }

    if (has_conflict) {
        Logger::instance().verbose("Action '" + action.name() +
            "' (id=" + std::to_string(action.id()) +
            ") has non-exclusive conditional effects — forcing interference");
    }

    conflicting_effects_cache_[action.id()] = has_conflict;
    return has_conflict;
}

} // namespace rantanplan