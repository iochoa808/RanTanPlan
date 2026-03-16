#pragma once

#include <vector>
#include <unordered_map>
#include "z3++.h"
#include "../problem/problem.hpp"
#include "../problem/expr_pool.hpp"
#include "../config/strategy_spec.hpp"

namespace rantanplan {

class BaseEncoder;
class InterferenceAnalysis;

/// Encodes an optimistic abstract suffix for branch-and-bound cost-optimal planning.
///
/// Given a concrete prefix of n steps (states 0..n, actions at steps 0..n-1), the
/// abstract suffix approximates all possible plan continuations beyond state n using
/// a single abstract layer. This yields a lower bound on the remaining cost, enabling
/// the B&B planner to prove global optimality: if no plan with total_cost < C* is
/// feasible (even with the optimistic suffix), then C* is optimal across ALL horizons.
///
/// Based on the abstract suffix framework from:
///   Leofante et al., "Optimal Planning Modulo Theories", IJCAI 2020.
///
/// The abstract suffix consists of:
///   - Abstract action variables (a^abs): one Boolean per grounded action, representing
///     a possible execution in the suffix (without committing to a specific timestep).
///   - Modified flags (mod_v): one Boolean per grounded fluent, true iff some abstract
///     action could modify that fluent.
///   - Relaxed preconditions (Eq. 4A): a^abs => AND_phi (phi_n OR OR_{v in vars(phi)} mod_v)
///   - mod_v equivalences (Eq. 4B): mod_v <=> OR{a^abs : a affects v} (from epc_index)
///   - Abstract goal (Eq. 3): each goal g relaxed to (g_n OR OR_{v in vars(g)} mod_v)
///   - Loop formulas (Eq. 5): SCC-based anti-circular-support constraints preventing
///     abstract actions from mutually justifying each other without external support.
///   - Axiom 6: no arbitrary deferral — each concrete action at step i must be justified.
///   - Axiom 7: full concrete prefix — if any abstract action fires, every concrete
///     step must have at least one action.
///
/// The API follows the same decomposed per-method pattern as BaseEncoder:
/// each method returns a z3::expr and never touches the solver. The planner
/// decides which constraints are permanent, incremental, or assumption-gated.
///
///   Permanent (add once):     encode_mod_v_equivalences(), encode_loop_formulas()
///   Incremental (per step):   encode_axiom_6(i), encode_axiom_7_step(i)
///   Assumption-gated:         encode_relaxed_preconditions(n), encode_abstract_goal(n)
///
/// The abstract cost uses each action's actual cost as a lower bound. For constant
/// costs this is exact; for SDAC, lower bounds are provided externally (computed
/// from NumericRelaxedPlanningGraph interval analysis at fixpoint). This is sound:
/// RPG fixpoint bounds over-approximate all reachable fluent values, so
/// min(cost_expr) over those bounds ≤ true minimum.
class AbstractSuffixEncoder {
public:
    AbstractSuffixEncoder(const Problem& problem, z3::context& ctx,
                          BaseEncoder& encoder,
                          const InterferenceAnalysis* interference,
                          SemanticsKind semantics);

    /// Set SDAC cost lower bounds (indexed by action position in problem.actions()).
    /// Must be called before search if the problem has state-dependent costs.
    void set_cost_lower_bounds(std::vector<double> bounds) { cost_lower_bounds_ = std::move(bounds); }

    /// Create abstract variables (a^abs, mod_v). Call once before the search loop.
    void initialize();

    // =========================================================================
    // Permanent constraints — add once after initialize()
    // =========================================================================

    /// Eq. 4B: mod_v <=> OR{a^abs : a affects v} for each grounded fluent.
    /// Horizon-independent (only references abstract vars and epc_index).
    z3::expr encode_mod_v_equivalences();

    /// Eq. 5: SCC-based anti-circular-support constraints.
    /// Horizon-independent (depends only on action structure). Cached internally.
    z3::expr encode_loop_formulas();

    // =========================================================================
    // Per-step incremental constraints — add once per new concrete step
    // =========================================================================

    /// Eq. 6: no arbitrary deferral at concrete step i (i > 0).
    /// Only references concrete action vars at steps i and i-1.
    z3::expr encode_axiom_6(int i);

    /// Eq. 7 (per-step): (OR a^abs) => (OR_a a_i).
    /// Individual clause — conjunction over all steps is equivalent to original Eq. 7.
    z3::expr encode_axiom_7_step(int i);

    // =========================================================================
    // Horizon-dependent constraints — planner gates with assumption literal
    // =========================================================================

    /// Eq. 4A: relaxed preconditions evaluated at state n.
    /// a^abs => AND_{phi in pre_a} (phi_n OR OR_{v in vars(phi)} mod_v)
    z3::expr encode_relaxed_preconditions(int n);

    /// Eq. 3: abstract goal — each goal relaxed with mod_v disjunction at state n.
    z3::expr encode_abstract_goal(int n);

    // =========================================================================
    // Cost and solution queries
    // =========================================================================

    /// Build and return the total cost expression: sum of concrete action costs at
    /// steps 0..n-1, plus abstract cost (actual action costs for constant, 0 for SDAC).
    z3::expr build_total_cost(int n);

    /// Return true if the model assigns false to all abstract action variables,
    /// meaning the plan is fully concrete and executable.
    bool is_concrete_solution(const z3::model& model) const;

private:
    const Problem& problem_;
    z3::context& ctx_;
    BaseEncoder& encoder_;
    const InterferenceAnalysis* interference_;
    SemanticsKind semantics_;

    /// Maps fluent ExprID -> list of (action, effect) pairs that can modify it.
    /// Borrowed from GroundedEncoder::get_epc_index(); used to build mod_v equivalences.
    const std::unordered_map<ExprID, std::vector<std::pair<const Action*, const EffectExpression*>>>* epc_index_;

    std::vector<double> cost_lower_bounds_;  ///< SDAC cost lower bounds (indexed by action position); empty if constant costs
    std::vector<z3::expr> action_abs_;      ///< One Bool per action: "action a fires in the abstract suffix"
    std::vector<z3::expr> mod_vars_;        ///< One Bool per grounded fluent: "fluent v may be modified by abstract suffix"
    std::unordered_map<ExprID, size_t> fluent_to_mod_index_; ///< Fluent ExprID -> index in mod_vars_

    z3::expr cached_loop_formulas_;         ///< Cached loop formula constraints (depend only on action structure)
    bool loop_formulas_built_ = false;

    /// Cost of concrete actions at step i: sum of ITE(a_i, cost_a, 0)
    z3::expr build_concrete_cost_term(int i);
    /// Abstract cost: sum of ITE(a^abs, c_a, 0) using actual action costs (or 0 for SDAC)
    z3::expr build_abstract_cost_term();

    /// Decompose a precondition ExprID into its conjuncts (handles AND nodes and atoms)
    std::vector<ExprID> get_precondition_conjuncts(ExprID precond_id) const;
    /// Collect mod_v variables for all fluents referenced in an expression
    z3::expr_vector get_mod_vars_for_expr(ExprID expr_id) const;
};

} // namespace rantanplan
