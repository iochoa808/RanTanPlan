#pragma once

#include "interference_analysis.hpp"
#include "../grounded_encoding_visitor.hpp"
#include "../z3_variable_factory.hpp"
#include <z3++.h>
#include <utility>

namespace rantanplan {

/**
 * @brief Eager semantic interference analysis implementation
 *
 * Pre-computes all action interferences using semantic analysis via Z3 SMT solver
 * and builds a complete interference graph. Provides fast O(1) lookup for
 * interference queries but uses more memory and has higher initialization cost.
 *
 * Uses semantic SMT queries based on the paper "Relaxing non-interference
 * requirements in parallel plans" to compute interferences during initialization,
 * then disposes of Z3 infrastructure to minimize runtime memory footprint.
 *
 * Supports all graph-based methods including get_interference_graph() and
 * get_neighbours().
 */
class EagerSemanticInterferenceAnalysis : public InterferenceAnalysis {
public:
    /**
     * @brief Construct and initialize the analyzer with a planning problem
     * @param problem The planning problem to analyze
     */
    explicit EagerSemanticInterferenceAnalysis(const Problem& problem);

    // Default constructor for cases where problem isn't available at construction time
    EagerSemanticInterferenceAnalysis() = default;
    
    void initialize(const Problem& problem) override;
    
    // Direct interference checking methods (fully supported)
    bool has_interference(const Action& a1, const Action& a2) const override;
    bool has_interference(int node_id1, int node_id2) const override;
    std::vector<const Action*> topological_sort_actions(const std::vector<const Action*>& actions) const override;

    // Graph-based methods (fully supported by eager analysis)
    const Graph& get_interference_graph() const override;
    const std::vector<int>& get_neighbours(int node_id) const override;

    // Utility methods
    bool is_lazy() const override { return false; }
    void output_interference_graph_dot(const std::string& filename = "interference.dot") const override;

private:
    // Interference graph storing pre-computed semantic interferences
    Graph interference_graph_;

    // Z3 context and solver for semantic checks (only used during initialization)
    // Mutable because they're accessed from const methods during graph building
    mutable std::unique_ptr<z3::context> z3_context_;
    mutable std::unique_ptr<z3::solver> z3_solver_;

    // Z3 infrastructure for expression conversion (only used during initialization)
    // Mutable because they're accessed from const methods during graph building
    mutable std::unique_ptr<Z3VariableFactory> z3_variable_factory_;
    mutable std::unique_ptr<GroundedEncodingVisitor> grounded_visitor_;

    /**
     * @brief Build the interference graph using semantic analysis
     *
     * Analyzes all action pairs using Z3-based semantic checks and creates edges
     * in the interference graph. After completion, disposes of Z3 infrastructure.
     */
    void build_interference_graph();

    /**
     * @brief Analyze conflicts between all pairs of actions using semantic analysis
     */
    void analyze_action_conflicts();
    
    
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
    z3::expr convert_expr_id_to_z3(ExprID eid) const;
    
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
    bool assignments_commute(const EffectExpression& eff1, const EffectExpression& eff2, ExprID var_eid) const;
    
    /**
     * @brief Convert an effect expression to a Z3 expression given a base variable
     * @param effect The effect expression to convert
     * @param base_var_z3 The Z3 expression representing the variable being affected
     * @return Z3 expression representing the effect applied to the base variable
     */
    z3::expr convert_effect_to_z3(const EffectExpression& effect, const z3::expr& base_var_z3) const;
    
};

} // namespace rantanplan