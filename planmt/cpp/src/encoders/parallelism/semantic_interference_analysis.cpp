#include "semantic_interference_analysis.h"
#include "../../util/memory_tracker.h"
#include "../../config/config.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <unordered_set>

namespace planmt {

SemanticInterferenceAnalysis::SemanticInterferenceAnalysis(const Problem& problem) {
    initialize(problem);
}

void SemanticInterferenceAnalysis::initialize(const Problem& problem) {
    problem_ = &problem;
    
    // Clear any existing data
    action_analysis_.clear();
    semantic_cache_.clear();
    
    // Setup common functionality using base class methods
    setup_action_node_mapping();
    analyze_all_actions();
    
    // Initialize Z3 context immediately
    z3_context_ = std::make_unique<z3::context>();
    z3_solver_ = std::make_unique<z3::solver>(*z3_context_);
    
    // Create grounded visitor for expression conversion  
    z3_variable_factory_ = std::make_unique<Z3VariableFactory>(*z3_context_);
    grounded_visitor_ = std::make_unique<GroundedEncodingVisitor>(*z3_context_, problem_, z3_variable_factory_.get());

    // Report memory usage after initialization
    double current_memory = MemoryTracker::instance().get_current_memory_mb();
    std::cout << "SemanticInterferenceAnalysis initialized with " << problem.actions().size() 
              << " actions and Z3 context. "
              << "Memory: " << current_memory << " MB" << std::endl;
}

bool SemanticInterferenceAnalysis::has_interference(const Action& a1, const Action& a2) const {
    // This implements the directional "affects" relationship from Definition 3.9
    // a1 affects a2. Remember that this is NOT symmetric
    return action_affects_semantically(a1, a2);
}

bool SemanticInterferenceAnalysis::has_interference(Graph::NodeId node_id1, Graph::NodeId node_id2) const {
    // Convert node IDs back to actions, then check if action1 affects action2
    const Action* action1 = get_action_from_node_id(node_id1);
    const Action* action2 = get_action_from_node_id(node_id2);
    return action_affects_semantically(*action1, *action2);
}

bool SemanticInterferenceAnalysis::action_affects_semantically(const Action& a1, const Action& a2) const {
    bool affects = false; // by default a1 does not affect a2

    // Create directional cache key for "affects" relationship
    std::string cache_key = a1.name() + " affects " + a2.name();
    
    // Check if result is already cached
    if (semantic_cache_.contains(cache_key)) return semantic_cache_.at(cache_key);
    
    // Based on Definition 3.9 from the paper:
    // Action a affects action b if either:
    // 1. a can prevent b's execution (pre(b) ∧ σ(a, ¬pre(b)) is satisfiable)
    // 2. a's effects don't commute properly with b's effects
    
    // Check condition 1: prevention of execution
    if (check1(a1, a2)) {
        affects = true;
    } else {
        // Check condition 2: effect commutativity
        affects = check2(a1, a2);
    }
    
    // Cache the directional "affects" result
    semantic_cache_[cache_key] = affects;
    return affects;
}



z3::expr SemanticInterferenceAnalysis::apply_action_effects_substitution(const Action& action, const z3::expr& target_expr) const {
    // Apply action's effects as substitution to the a2 expression
    // This implements the σ(action, expr) operation from the paper
    if (action.effects().empty()) return target_expr; // No effects to apply
    
    std::vector<z3::expr> from_exprs;
    std::vector<z3::expr> to_exprs;
    
    // Build substitution pairs from action effects
    for (const Effect& effect : action.effects()) {
        const EffectExpression& eff_expr = effect.effect_expression();
        
        // Convert fluent to Z3
        z3::expr fluent_z3 = convert_expression_to_z3(eff_expr.fluent());
        
        // Create the new value expression based on effect type
        z3::expr new_value_z3 = convert_effect_to_z3(eff_expr, fluent_z3);
        
        // Add substitution: fluent -> new_value
        from_exprs.push_back(fluent_z3);
        to_exprs.push_back(new_value_z3);
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

z3::expr SemanticInterferenceAnalysis::convert_expression_to_z3(const Expression& expr) const {
    grounded_visitor_->clear();
    // No timestep needed for semantic interference - we work with current state
    grounded_visitor_->clear_timestep();
    accept_visitor(expr, *grounded_visitor_);
    
    auto result = grounded_visitor_->get_result();
    if (!result) {
        throw std::runtime_error("Failed to convert expression to Z3: " + expr.to_string());
    }
    
    return *result;
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
    if (!are_simply_commuting(a1, a2)) return true; // Not simply commuting, so a1 affects a2
    
    // Second check: do effects commute properly?
    // We need to check if pre(a1) ∧ pre(a2) ∧ ¬(x^{σ_h({a1,a2})} = x^{σ_a2 ∘ σ_a1}) is satisfiable
    
    // Get preconditions
    z3::expr a1_pre = convert_precondition_to_z3(a1);
    z3::expr a2_pre = convert_precondition_to_z3(a2);
    
    // Check all variables that could be affected by either action
    std::unordered_set<Expression> affected_vars;
    
    // Collect variables from effects
    for (const Effect& effect : a1.effects()) { affected_vars.insert(effect.effect_expression().fluent()); }
    for (const Effect& effect : a2.effects()) { affected_vars.insert(effect.effect_expression().fluent()); }
    
    // For each affected variable, check if happening and sequential execution differ
    for (const Expression& var : affected_vars) {
        z3::expr var_z3 = convert_expression_to_z3(var);
        
        // Create happening effect: apply a1 and a2 in parallel
        z3::expr var_after_happening = var_z3;
        var_after_happening = apply_action_effects_substitution(a2, var_after_happening);
        var_after_happening = apply_action_effects_substitution(a1, var_after_happening);
        
        // Create sequential effect: apply a2 after a1 (σ_target ∘ σ_source)
        z3::expr var_after_sequential = var_z3;
        var_after_sequential = apply_action_effects_substitution(a1, var_after_sequential);
        var_after_sequential = apply_action_effects_substitution(a2, var_after_sequential);
        
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
    
    // Build maps from variables to their effects for both actions
    std::unordered_map<Expression, const EffectExpression*> a1_effects;
    std::unordered_map<Expression, const EffectExpression*> a2_effects;
    
    for (const Effect& effect : a1.effects()) {
        a1_effects[effect.effect_expression().fluent()] = &effect.effect_expression();
    }
    for (const Effect& effect : a2.effects()) {
        a2_effects[effect.effect_expression().fluent()] = &effect.effect_expression();
    }
    
    // Check commutativity for each variable modified by both actions
    for (const auto& [var, a1_effect] : a1_effects) {
        auto a2_it = a2_effects.find(var);
        if (a2_it != a2_effects.end()) {
            auto a2_effect = a2_it->second;
            // Both actions modify this variable - check if assignments commute
            if (!assignments_commute(*a1_effect, *a2_effect, var)) {
                return false;
            }
        }
    }
    
    return true; // All common variables commute
}

bool SemanticInterferenceAnalysis::assignments_commute(const EffectExpression& eff1,
                                                       const EffectExpression& eff2,
                                                       const Expression& var) const {
    // Check if two assignments to the same variable commute
    // According to Definition 3.3: T ⊨ (exp2{x → exp1} = exp1{x → exp2})
    
    z3::expr var_z3 = convert_expression_to_z3(var);
    
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

Graph::NodeId SemanticInterferenceAnalysis::get_action_node_id(const Action& action) const {
    auto it = action_to_node_id_.find(action);
    return (it != action_to_node_id_.end()) ? it->second : -1;
}

const Action* SemanticInterferenceAnalysis::get_action_from_node_id(Graph::NodeId node_id) const {
    if (node_id >= 0 && static_cast<size_t>(node_id) < node_id_to_action_.size()) {
        return node_id_to_action_[node_id];
    }
    return nullptr;
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
    std::vector<Graph::NodeId> node_ids;
    for (const Action* action : actions) {
        Graph::NodeId node_id = get_action_node_id(*action);
        if (node_id >= 0) { // Valid node ID found
            node_ids.push_back(node_id);
        }
    }
    
    if (node_ids.empty()) {
        return actions; // No valid node IDs found, return as-is
    }
    
    // Build a temporary subgraph using existing node IDs and cached semantic interferences
    Graph temp_subgraph(node_id_to_action_.size());
    
    // Add edges using cached semantic interference results
    for (size_t i = 0; i < actions.size(); ++i) {
        for (size_t j = 0; j < actions.size(); ++j) {
            if (i != j) {
                const Action* action1 = actions[i];
                const Action* action2 = actions[j];
                
                // Add directed edge if action1 affects action2
                if (has_interference(*action1, *action2)) {
                    Graph::NodeId node1 = get_action_node_id(*action1);
                    Graph::NodeId node2 = get_action_node_id(*action2);
                    if (node1 >= 0 && node2 >= 0) {
                        temp_subgraph.add_edge(node1, node2);
                    }
                }
            }
        }
    }
    
    // Use Graph's topological sort for execution order
    std::vector<Graph::NodeId> sorted_node_ids = temp_subgraph.topological_sort(node_ids);
    
    // Convert back to actions using existing base class mappings
    std::vector<const Action*> result;
    for (Graph::NodeId node_id : sorted_node_ids) {
        const Action* action = get_action_from_node_id(node_id);
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
    return convert_expression_to_z3(action.precondition());
}

z3::expr SemanticInterferenceAnalysis::convert_effect_to_z3(const EffectExpression& effect, const z3::expr& base_var_z3) const {
    switch (effect.kind()) {
        case EffectExpression::Kind::ASSIGN:
            return convert_expression_to_z3(effect.value());
        case EffectExpression::Kind::INCREASE:
            return base_var_z3 + convert_expression_to_z3(effect.value());
        case EffectExpression::Kind::DECREASE:
            return base_var_z3 - convert_expression_to_z3(effect.value());
    }
    throw std::runtime_error("Unknown effect kind");
}

// Graph-based methods (NOT supported by semantic lazy analysis)

const Graph& SemanticInterferenceAnalysis::get_interference_graph() const {
    throw std::runtime_error("SemanticInterferenceAnalysis does not support get_interference_graph(). "
                            "This method is only available with eager analysis. "
                            "Use has_interference() for individual interference checks instead.");
}

const std::vector<Graph::NodeId>& SemanticInterferenceAnalysis::get_neighbours(Graph::NodeId node_id) const {
    throw std::runtime_error("SemanticInterferenceAnalysis does not support get_neighbours(). "
                            "This method is only available with eager analysis. "
                            "Use has_interference() for individual interference checks instead.");
}

void SemanticInterferenceAnalysis::output_interference_graph_dot(const std::string& filename) const {
    throw std::runtime_error("SemanticInterferenceAnalysis does not support output_interference_graph_dot(). "
                            "This method is only available with eager analysis as it requires the full graph.");
}

} // namespace planmt