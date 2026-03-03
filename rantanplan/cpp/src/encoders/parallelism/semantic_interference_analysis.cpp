#include "semantic_interference_analysis.hpp"
#include "../../util/memory_tracker.hpp"
#include "../../config/config.hpp"
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
    
    // Setup common functionality using base class methods
    analyze_all_actions();
    
    // Initialize Z3 context immediately
    z3_context_ = std::make_unique<z3::context>();
    z3_solver_ = std::make_unique<z3::solver>(*z3_context_);
    
    // Create grounded visitor for expression conversion  
    z3_variable_factory_ = std::make_unique<Z3VariableFactory>(*z3_context_);
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
    
    // Based on Definition 3.9 from the paper:
    // Action a affects action b if either:
    // 1. a can prevent b's execution (pre(b) ∧ σ(a, ¬pre(b)) is satisfiable)
    // 2. a's effects don't commute properly with b's effects
    
    // Check condition 1: prevention of execution
    bool check1_result = check1(a1, a2);
    if (check1_result) {
        affects = true;
    } else {
        // Check condition 2: effect commutativity
        bool check2_result = check2(a1, a2);
        if (check2_result) {
            affects = true;
        }
    }
    
    // Cache the directional "affects" result
    semantic_cache_[cache_key] = affects;
    return affects;
}



z3::expr SemanticInterferenceAnalysis::apply_action_effects_substitution(const Action& action, const z3::expr& target_expr) const {
    // Apply action's effects as substitution to the a2 expression
    // This implements the σ(action, expr) operation from the paper
    if (action.effects().empty()) {
        return target_expr; // No effects to apply
    }
    
    std::vector<z3::expr> from_exprs;
    std::vector<z3::expr> to_exprs;
    
    // Build substitution pairs from action effects
    for (const Effect& effect : action.effects()) {
        const EffectExpression& eff_expr = effect.effect_expression();
        
        // Convert fluent to Z3
        z3::expr fluent_z3 = grounded_visitor_->convert_from_pool(eff_expr.fluent_id(), -1);

        // Create the new value expression based on effect type
        z3::expr new_value_z3 = convert_effect_to_z3(eff_expr, fluent_z3);

        // Handle conditional effects properly
        z3::expr substitution_value = new_value_z3;
        if (effect.is_conditional()) {
            // For conditional effects: fluent -> (condition ? new_value : fluent)
            z3::expr condition_z3 = grounded_visitor_->convert_from_pool(effect.effect_expression().condition_id(), -1);
            substitution_value = z3::ite(condition_z3, new_value_z3, fluent_z3);
        }
        
        // Add substitution: fluent -> substitution_value
        from_exprs.push_back(fluent_z3);
        to_exprs.push_back(substitution_value);
    }

    // I think this is not needed as if we have an effect, we will have substitutions to apply
    // if (from_exprs.empty()) return target_expr; // No substitutions to apply
    
    // Apply substitution using Z3's substitute function
    z3::expr_vector from_vector(*z3_context_);
    z3::expr_vector to_vector(*z3_context_);
    
    for (size_t i = 0; i < from_exprs.size(); ++i) {
        from_vector.push_back(from_exprs[i]);
        to_vector.push_back(to_exprs[i]);
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

bool SemanticInterferenceAnalysis::check2(const Action& a1, const Action& a2) const {
    // Condition 2: either actions are not simply commuting, or effects don't commute properly
    
    // First check: are they simply commuting?
    if (!are_simply_commuting(a1, a2)) {
        return true; // Not simply commuting, so a1 affects a2
    }
    
    // Second check: do effects commute properly?
    // We need to check if pre(a1) ∧ pre(a2) ∧ ¬(x^{σ_h({a1,a2})} = x^{σ_a2 ∘ σ_a1}) is satisfiable
    // Get preconditions
    z3::expr a1_pre = convert_precondition_to_z3(a1);
    z3::expr a2_pre = convert_precondition_to_z3(a2);
    
    // Check all variables that could be affected by either action (use ExprID to deduplicate)
    std::unordered_set<ExprID> affected_var_eids;

    for (const Effect& effect : a1.effects()) {
        affected_var_eids.insert(effect.effect_expression().fluent_id());
    }
    for (const Effect& effect : a2.effects()) {
        affected_var_eids.insert(effect.effect_expression().fluent_id());
    }

    // For each affected variable, check if happening and sequential execution differ
    for (ExprID var_eid : affected_var_eids) {
        z3::expr var_z3 = grounded_visitor_->convert_from_pool(var_eid, -1);
        
        // Create happening effect: apply a1 and a2 in parallel
        z3::expr var_after_happening = var_z3;
        var_after_happening = apply_action_effects_substitution(a2, var_after_happening);
        var_after_happening = apply_action_effects_substitution(a1, var_after_happening);
        
        // Create sequential effect: apply a2 after a1 (σ_target ∘ σ_source)
        z3::expr var_after_sequential = var_z3;
        var_after_sequential = apply_action_effects_substitution(a1, var_after_sequential);
        var_after_sequential = apply_action_effects_substitution(a2, var_after_sequential);
        
        // Check if the substitutions actually changed anything
        if (z3::eq(var_after_happening, var_after_sequential)) {
            continue; // No difference, so this variable commutes properly
        }
        
        // Check if pre(a1) ∧ pre(a2) ∧ ¬(happening = sequential) is satisfiable
        z3::expr non_commutativity = a1_pre && a2_pre && (var_after_happening != var_after_sequential);
        
        z3_solver_->push();
        z3_solver_->add(non_commutativity);
        z3::check_result result = z3_solver_->check();
        z3_solver_->pop();
        
        if (result == z3::sat) {
            return true; // Effects don't commute for this variable!
        }
    }
    
    return false; // All variables commute properly
}

bool SemanticInterferenceAnalysis::are_simply_commuting(const Action& a1, const Action& a2) const {
    // Two actions are simply commuting if for every variable x modified by both,
    // the assignments {x → exp1} and {x → exp2} commute
    
    // Build maps from variables (by ExprID) to their effects for both actions
    std::unordered_map<ExprID, const EffectExpression*> a1_effects;
    std::unordered_map<ExprID, const EffectExpression*> a2_effects;

    for (const Effect& effect : a1.effects()) {
        a1_effects[effect.effect_expression().fluent_id()] = &effect.effect_expression();
    }
    for (const Effect& effect : a2.effects()) {
        a2_effects[effect.effect_expression().fluent_id()] = &effect.effect_expression();
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

} // namespace rantanplan