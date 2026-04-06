#pragma once

#include "interference_analysis.hpp"
#include "../util/hash_utils.hpp"
#include "../encoders/grounded_encoding_visitor.hpp"
#include "../encoders/z3_variable_factory.hpp"
#include <z3++.h>
#include <utility>

namespace rantanplan {

/// Source of interference detected by semantic analysis.
/// Used by the state-aware propagator to decide whether a condition can be derived.
///
/// From Definition 3.9 of "Relaxing non-interference requirements in parallel plans"
/// (Bofill, Espasa, Villaret 2019):
///
///   CHECK1: Prevention — Pre_a ∧ Pre_b ∧ ¬(Pre_b σ_a) is T-satisfiable.
///     "a can impede execution of b."  STATE-DEPENDENT: the condition ¬(Pre_b σ_a)
///     may hold in some states but not others.
///
///   CHECK2_UNCOMM: Not simply commuting (Def 3.5/3.3) — the assignments themselves
///     don't commute algebraically.  STATE-INDEPENDENT: unconditional interference.
///
///   CHECK2_HAPPEN: Simply commuting but happening ≠ sequential (Def 3.6/3.7) —
///     Pre_a ∧ Pre_b ∧ ¬(x σ_{h({a,b})} = x σ_b σ_a) is T-satisfiable.
///     "Effects compose differently in parallel vs sequentially."  STATE-DEPENDENT.
///
///   CONFLICTING_COND_EFFECTS: Conservative fallback for actions with non-exclusive
///     conditional effects on the same fluent.  STATE-INDEPENDENT.
///
enum class InterferenceSource {
    NONE,
    CHECK1,           // state-dependent: ¬(Pre_b σ_a)
    CHECK2_UNCOMM,    // state-independent: assignments don't commute
    CHECK2_HAPPEN,    // state-dependent: happening ≠ sequential
    CONFLICTING_COND_EFFECTS  // state-independent: conservative
};

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
    bool has_interference(int node_id1, int node_id2) const override;
    std::vector<const Action*> topological_sort_actions(const std::vector<const Action*>& actions) const override;
    
    // Graph-based methods (NOT supported - will throw exceptions)
    const Graph& get_interference_graph() const override;
    const std::vector<int>& get_neighbours(int node_id) const override;
    
    // Utility methods
    bool is_lazy() const override { return true; }
    void output_interference_graph_dot(const std::string& filename = "interference.dot") const override;

    /// Return the source of interference for a previously-queried pair.
    /// Only valid after has_interference(source, target) has been called.
    InterferenceSource get_interference_source(int source_id, int target_id) const;

private:
    
    // Z3 context and solver for semantic checks
    mutable std::unique_ptr<z3::context> z3_context_;
    mutable std::unique_ptr<z3::solver> z3_solver_;
    
    // Z3 infrastructure for expression conversion (similar to GroundedEncoder)
    mutable std::unique_ptr<Z3VariableFactory> z3_variable_factory_;
    mutable std::unique_ptr<GroundedEncodingVisitor> grounded_visitor_;
    
    // Cache for semantic interference results to avoid recomputation
    // Keyed by (action_id_1, action_id_2) pair for correct per-instance caching
    mutable std::unordered_map<std::pair<int,int>, bool, PairHash> semantic_cache_;

    // Cache for interference source (what check produced the result)
    mutable std::unordered_map<std::pair<int,int>, InterferenceSource, PairHash> source_cache_;

    // Cache for per-action conflicting conditional effects check
    mutable std::unordered_map<int, bool> conflicting_effects_cache_;
    
    
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
     * @brief Check condition 1 of Definition 3.9: prevention of execution
     * @param source Source action
     * @param target Target action
     * @return True if source can prevent target's execution
     */
    bool check1(const Action& source, const Action& target) const;
    
    /**
     * @brief Check condition 2 of Definition 3.9: effect commutativity.
     * Returns the specific sub-case:
     *   CHECK2_UNCOMM:  not simply commuting (state-independent)
     *   CHECK2_HAPPEN:  simply commuting but happening ≠ sequential (state-dependent)
     *   NONE:           no interference from condition 2
     */
    InterferenceSource check2_source(const Action& source, const Action& target) const;
    
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

    /**
     * @brief Check if an action has non-exclusive conditional effects on any fluent.
     *
     * For each fluent with multiple conditional effects, checks whether
     * pre(action) ∧ cond_i ∧ cond_j is satisfiable for any pair (i,j).
     * If so, the nested ite composition may not faithfully model the effects,
     * and interference should be reported conservatively.
     * Results are cached per action ID.
     */
    bool has_conflicting_conditional_effects(const Action& action) const;

};

} // namespace rantanplan