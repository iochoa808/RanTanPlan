#pragma once

#include "interference_analysis.h"
#include "../grounded_encoding_visitor.h"
#include "../z3_variable_factory.h"
#include <z3++.h>
#include <utility>

namespace planmt {

/**
 * @brief Semantic interference analysis implementation
 * 
 * Computes action interferences using semantic analysis via Z3 SMT solver.
 * Uses on-demand computation and caching similar to LazyInterferenceAnalysis,
 * but replaces syntactic checks with semantic SMT queries based on the paper
 * "Relaxing non-interference requirements in parallel plans".
 * 
 * WARNING: This implementation does NOT support graph-based methods like
 * get_interference_graph() or get_neighbours(). These methods will throw exceptions.
 */
class SemanticInterferenceAnalysis : public InterferenceAnalysis {
public:
    /**
     * @brief Construct and initialize the analyzer with a planning problem
     * @param problem The planning problem to analyze
     */
    explicit SemanticInterferenceAnalysis(const Problem& problem);
    
    // Default constructor for cases where problem isn't available at construction time
    SemanticInterferenceAnalysis() = default;
    
    void initialize(const Problem& problem) override;
    
    // Direct interference checking methods (fully supported)
    bool has_interference(const Action& a1, const Action& a2) const override;
    bool has_interference(Graph::NodeId node_id1, Graph::NodeId node_id2) const override;
    Graph::NodeId get_action_node_id(const Action& action) const override;
    const Action* get_action_from_node_id(Graph::NodeId node_id) const override;
    std::vector<const Action*> topological_sort_actions(const std::vector<const Action*>& actions) const override;
    
    // Graph-based methods (NOT supported - will throw exceptions)
    const Graph& get_interference_graph() const override;
    const std::vector<Graph::NodeId>& get_neighbours(Graph::NodeId node_id) const override;
    
    // Utility methods
    bool is_lazy() const override { return true; }
    void output_interference_graph_dot(const std::string& filename = "interference.dot") const override;

private:
    
    // Z3 context and solver for semantic checks
    mutable std::unique_ptr<z3::context> z3_context_;
    mutable std::unique_ptr<z3::solver> z3_solver_;
    
    // Z3 infrastructure for expression conversion (similar to GroundedEncoder)
    mutable std::unique_ptr<Z3VariableFactory> z3_variable_factory_;
    mutable std::unique_ptr<GroundedEncodingVisitor> grounded_visitor_;
    
    // Cache for semantic interference results to avoid recomputation
    // Uses string key based on action names for simplicity
    mutable std::unordered_map<std::string, bool> semantic_cache_;
    
    
    /**
     * @brief Check if source action affects target action semantically
     * Based on Definition 3.9 from the paper:
     * - Condition 1: source can prevent target execution
     * - Condition 2: effects don't commute properly
     * @param source Source action
     * @param target Target action
     * @return True if source affects target
     */
    bool action_affects_semantically(const Action& source, const Action& target) const;
    
    /**
     * @brief Convert action precondition to Z3 expression
     * @param action Action to convert precondition for
     * @return Z3 expression representing the precondition
     */
    z3::expr convert_precondition_to_z3(const Action& action) const;
    
    /**
     * @brief Apply action effects as substitution to target expression
     * Uses Z3's substitute function to apply effects
     * @param action Action whose effects to apply
     * @param target_expr Expression to substitute into
     * @return Expression with effects applied
     */
    z3::expr apply_action_effects_substitution(const Action& action, const z3::expr& target_expr) const;
    
    /**
     * @brief Convert planning expression to Z3 expression using the visitor
     * @param expr Planning expression to convert
     * @return Z3 expression
     */
    z3::expr convert_expression_to_z3(const Expression& expr) const;
    
    /**
     * @brief Check condition 1 of Definition 3.9: prevention of execution
     * @param source Source action
     * @param target Target action
     * @return True if source can prevent target's execution
     */
    bool check1(const Action& source, const Action& target) const;
    
    /**
     * @brief Check condition 2 of Definition 3.9: effect commutativity
     * @param source Source action
     * @param target Target action
     * @return True if source affects target due to non-commutativity
     */
    bool check2(const Action& source, const Action& target) const;
    
    /**
     * @brief Check if two actions are simply commuting (all their effects commute)
     * @param a1 First action
     * @param a2 Second action
     * @return True if actions are simply commuting
     */
    bool are_simply_commuting(const Action& a1, const Action& a2) const;
    
    /**
     * @brief Check if two assignments to the same variable commute
     * @param eff1 First effect expression
     * @param eff2 Second effect expression
     * @param var The variable being assigned
     * @return True if assignments commute
     */
    bool assignments_commute(const EffectExpression& eff1, const EffectExpression& eff2, const Expression& var) const;
    
    /**
     * @brief Convert an effect expression to a Z3 expression given a base variable
     * @param effect The effect expression to convert
     * @param base_var_z3 The Z3 expression representing the variable being affected
     * @return Z3 expression representing the effect applied to the base variable
     */
    z3::expr convert_effect_to_z3(const EffectExpression& effect, const z3::expr& base_var_z3) const;
    
};

} // namespace planmt