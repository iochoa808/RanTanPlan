#include "eager_semantic_interference_analysis.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include "../config/config.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <unordered_set>
#include <fstream>

namespace rantanplan {

EagerSemanticInterferenceAnalysis::EagerSemanticInterferenceAnalysis(const Problem& problem) {
    initialize(problem);
}

void EagerSemanticInterferenceAnalysis::initialize(const Problem& problem) {
    problem_ = &problem;

    // Clear any existing data
    action_analysis_.clear();
    interference_graph_ = Graph();

    // Setup common functionality using base class methods
    analyze_all_actions();

    // Create nodes in the interference graph to match action IDs
    for (size_t i = 0; i < problem.actions().size(); ++i) {
        interference_graph_.add_node();
    }

    // Initialize Z3 context for semantic analysis during graph building
    z3_context_ = std::make_unique<z3::context>();
    z3_solver_ = std::make_unique<z3::solver>(*z3_context_);

    // Create grounded visitor for expression conversion
    z3_variable_factory_ = std::make_unique<Z3VariableFactory>(*z3_context_);
    grounded_visitor_ = std::make_unique<GroundedEncodingVisitor>(*z3_context_, problem_, z3_variable_factory_.get());

    // Report initialization completion
    double current_memory = MemoryTracker::instance().get_current_memory_mb();
    Logger::instance().verbose("EagerSemanticInterferenceAnalysis initialized with " + std::to_string(problem.actions().size()) +
                              " actions and Z3 infrastructure (memory: " + std::to_string(static_cast<int>(current_memory)) + "MB)");

    // Build interference graph using semantic analysis
    build_interference_graph();
}

void EagerSemanticInterferenceAnalysis::build_interference_graph() {
    if (!problem_) {
        Logger::instance().error("EagerSemanticInterferenceAnalysis not initialized with a problem");
        return;
    }

    ScopedTimer timer("interference.semantic.build_time_ms");
    double start_memory = MemoryTracker::instance().get_current_memory_mb();

    analyze_action_conflicts();

    // Record stats
    double memory_used = MemoryTracker::instance().get_current_memory_mb() - start_memory;
    Stats::instance().set("interference.semantic.nodes", interference_graph_.num_nodes());
    Stats::instance().set("interference.semantic.edges", interference_graph_.num_edges());
    Stats::instance().set("interference.semantic.memory_mb", memory_used);

    // Structured visual output
    Logger::instance().component(VerbosityLevel::INFO, "Interference.S", {
        {"time", std::to_string(static_cast<int>(timer.elapsed_ms())) + "ms"},
        {"nodes", std::to_string(interference_graph_.num_nodes())},
        {"edges", std::to_string(interference_graph_.num_edges())},
        {"mem", std::to_string(static_cast<int>(memory_used)) + "MB"}
    });

    // Dispose of Z3 infrastructure to minimize runtime memory footprint
    Logger::instance().verbose("Disposing of Z3 infrastructure");
    grounded_visitor_.reset();
    z3_variable_factory_.reset();
    z3_solver_.reset();
    z3_context_.reset();

    double memory_after_cleanup = MemoryTracker::instance().get_current_memory_mb();
    Logger::instance().verbose("Z3 infrastructure disposed (memory: " + std::to_string(static_cast<int>(memory_after_cleanup)) + "MB)");
}

void EagerSemanticInterferenceAnalysis::analyze_action_conflicts() {
    const auto& actions = problem_->actions();

    // Expensive O(n²) preprocessing: analyze all pairs of actions for semantic conflicts
    // Results are cached in the interference graph for fast lookup during execution
    for (size_t i = 0; i < actions.size(); ++i) {
        for (size_t j = 0; j < actions.size(); ++j) {
            if (i != j) {  // Don't check action against itself
                const Action& action1 = actions[i];
                const Action& action2 = actions[j];

                if (action_affects_semantically(action1, action2)) {
                    // Cache directional semantic interference: action1 affects action2
                    int node1 = action1.id();
                    int node2 = action2.id();
                    interference_graph_.add_edge(node1, node2);
                }
            }
        }
    }
}

bool EagerSemanticInterferenceAnalysis::has_interference(const Action& a1, const Action& a2) const {
    // DIRECTIONAL CHECK: Does a1 interfere with a2? (a1 -> a2)
    // This is NOT symmetric: has_interference(A,B) != has_interference(B,A) in general
    // Uses pre-computed interference graph for O(1) lookup
    return interference_graph_.has_edge(a1.id(), a2.id());
}

bool EagerSemanticInterferenceAnalysis::has_interference(int node_id1, int node_id2) const {
    // DIRECTIONAL CHECK: Does node_id1 interfere with node_id2? (node_id1 -> node_id2)
    // This is NOT symmetric: has_interference(A,B) != has_interference(B,A) in general
    // This optimized version works directly with node IDs, avoiding Action object lookups
    return interference_graph_.has_edge(node_id1, node_id2);
}

bool EagerSemanticInterferenceAnalysis::action_affects_semantically(const Action& a1, const Action& a2) const {
    // Based on Definition 3.9 from the paper:
    // Action a affects action b if either:
    // 1. a can prevent b's execution (pre(b) ∧ σ(a, ¬pre(b)) is satisfiable)
    // 2. a's effects don't commute properly with b's effects

    // Check condition 1: prevention of execution
    if (check1(a1, a2)) {
        return true;
    }

    // Check condition 2: effect commutativity
    return check2(a1, a2);
}



z3::expr EagerSemanticInterferenceAnalysis::apply_action_effects_substitution(const Action& action, const z3::expr& target_expr) const {
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

bool EagerSemanticInterferenceAnalysis::check1(const Action& a1, const Action& a2) const {
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

bool EagerSemanticInterferenceAnalysis::check2(const Action& a1, const Action& a2) const {
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

bool EagerSemanticInterferenceAnalysis::are_simply_commuting(const Action& a1, const Action& a2) const {
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

bool EagerSemanticInterferenceAnalysis::assignments_commute(const EffectExpression& eff1,
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


std::vector<const Action*> EagerSemanticInterferenceAnalysis::topological_sort_actions(const std::vector<const Action*>& actions) const {
    std::vector<const Action*> result;

    if (actions.size() <= 1) {
        result = actions; // No sorting needed
    } else {
        // Convert actions to node IDs
        std::vector<int> node_ids;

        for (const Action* action : actions) {
            int node_id = action->id();
            if (node_id >= 0) { // Valid node ID
                node_ids.push_back(node_id);
            }
        }

        if (node_ids.empty()) {
            result = actions; // No valid node IDs found
        } else {
            // Use the pre-computed interference graph's topological sort
            std::vector<int> sorted_node_ids = interference_graph_.topological_sort(node_ids);

            // Convert back to actions using direct problem access
            for (int node_id : sorted_node_ids) {
                if (node_id >= 0 && static_cast<size_t>(node_id) < problem_->action_count()) {
                    result.push_back(&problem_->action(node_id));
                }
            }
        }
    }

    return result;
}


z3::expr EagerSemanticInterferenceAnalysis::convert_precondition_to_z3(const Action& action) const {
    if (!action.has_precondition()) {
        return z3_context_->bool_val(true);
    }
    return grounded_visitor_->convert_from_pool(action.precondition_id(), -1);
}

z3::expr EagerSemanticInterferenceAnalysis::convert_effect_to_z3(const EffectExpression& effect, const z3::expr& base_var_z3) const {
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

// Graph-based methods (fully supported by eager semantic analysis)

const Graph& EagerSemanticInterferenceAnalysis::get_interference_graph() const {
    return interference_graph_;
}

const std::vector<int>& EagerSemanticInterferenceAnalysis::get_neighbours(int node_id) const {
    return interference_graph_.get_neighbours(node_id);
}

void EagerSemanticInterferenceAnalysis::output_interference_graph_dot(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        Logger::instance().error("Could not open file " + filename + " for writing");
        return;
    }

    Logger::instance().info("Writing semantic interference graph to " + filename);

    file << "digraph SemanticInterferenceGraph {" << std::endl;
    file << "    rankdir=LR;" << std::endl;
    file << "    node [shape=box, style=rounded];" << std::endl;
    file << "    edge [color=blue, arrowhead=vee];" << std::endl;
    file << std::endl;

    // Write nodes (actions)
    for (size_t i = 0; i < problem_->action_count(); ++i) {
        const Action& action = problem_->action(i);
        file << "    " << i << " [label=\"" << action.name() << "\"];" << std::endl;
    }

    file << std::endl;

    // Write edges (semantic interferences)
    for (int node_id = 0; node_id < static_cast<int>(problem_->action_count()); ++node_id) {
        const auto& neighbors = interference_graph_.get_neighbours(node_id);
        for (int neighbor : neighbors) {
            file << "    " << node_id << " -> " << neighbor << ";" << std::endl;
        }
    }

    file << "}" << std::endl;
    file.close();

    Logger::instance().info("Semantic interference graph successfully written to " + filename);
}

} // namespace rantanplan