#pragma once

#include "base_planner.hpp"
#include "../encoders/abstract_suffix_encoder.hpp"
#include "../config/strategy_spec.hpp"
#include <memory>

namespace rantanplan {

class BaseEncoder;
class InterferenceAnalysis;

/// Cost-optimal planner using branch-and-bound with an abstract suffix lower bound.
///
/// Instead of z3::optimize (which is incompatible with Z3 user propagators), this
/// planner reformulates cost minimization as a sequence of SAT feasibility checks
/// with progressively tighter cost bounds, using z3::solver throughout. This preserves
/// full compatibility with forall/exists/lazy propagators.
///
/// Algorithm (outer loop over horizons, inner B&B loop per horizon):
///   1. Encode initial state, add permanent abstract constraints (mod_v, loop formulas).
///   2. For each scheduled horizon h:
///      a. Fill concrete transitions up to h (permanent — outside push/pop scope).
///      b. solver.push() — open scope for horizon-dependent suffix.
///      c. Add axiom 6 + 7 per-step constraints, relaxed preconditions, abstract goal.
///      d. Build total_cost = sum(concrete costs) + abstract cost.
///      e. Inner loop: assert total_cost < C* via assumption literal, check SAT.
///         - SAT + all-concrete: update C* (upper bound), save plan, continue inner loop.
///         - SAT + abstract actions used: break to next horizon (need more concrete steps).
///         - UNSAT + no plan yet: break to next horizon (or UNSOLVABLE_PROVEN at h=0).
///         - UNSAT + have plan: C* is GLOBALLY OPTIMAL — return.
///      f. solver.pop() — discard horizon-scoped constraints.
///   3. Return best plan found (SOLVED_SATISFICING if optimality not proven within limits).
///
/// The AbstractSuffixEncoder provides a decomposed API (mirroring BaseEncoder's
/// per-timestep pattern) that separates permanent constraints (mod_v equivalences,
/// loop formulas) from horizon-scoped ones (axioms, relaxed preconditions, abstract
/// goal). This avoids re-adding the permanent parts at each horizon.
///
/// Key insight: UNSAT with cost bound proves no plan of cost < C* exists at ANY horizon,
/// because the abstract suffix is an optimistic approximation of all future steps.
///
/// Based on: Leofante et al., "Optimal Planning Modulo Theories", IJCAI 2020.
///
/// Cost behavior depends on the problem's metric and action cost configuration:
///
///   Metric                    | Action costs | What B&B minimizes
///   --------------------------+--------------+-----------------------------------
///   MINIMIZE_ACTION_COSTS     | constant     | Total action cost (normal case)
///   MINIMIZE_ACTION_COSTS     | SDAC         | Total action cost (state-dependent,
///   (state-dependent)         |              | non-linear terms — correct but slow)
///   MINIMIZE_PLAN_LENGTH      | —            | Number of actions (default cost = 1)
///   NONE                      | —            | Number of actions (default cost = 1)
///
/// When no explicit action costs are defined, build_concrete_cost_term() defaults
/// to cost = 1 per action, so B&B effectively minimizes plan length. This applies
/// both to MINIMIZE_PLAN_LENGTH metrics and to problems with no metric at all.
///
/// SDAC correctness and parallelism:
///
/// The concrete cost term evaluates action costs via convert_cost_to_z3(), which
/// by default evaluates at the start-of-timestep state x_t. This is correct when
/// each action genuinely "sees" x_t:
///
///   - Sequential semantics: one action per step, x_t is the exact state. Correct.
///   - Forall semantics: all actions execute simultaneously seeing x_t. Correct.
///   - R2E semantics: fixed declaration order with chain variables. Each action a_i
///     sees state σ^t_{prev(i)} (the state after all prior actions in the fixed
///     order). R2EGroundedEncoder::convert_cost_to_z3() applies prev_substitution
///     to evaluate costs at this intermediate state, making SDAC sound.
///
/// However, exists-step semantics allows interfering actions at the same timestep
/// as long as there exists a valid serialization. In that serialization, each
/// action sees a different intermediate state — not x_t. The encoding evaluates
/// costs at x_t regardless, which can under-estimate the true serialized cost.
/// This can lead to false optimality claims (e.g., encoding computes cost 25 but
/// the serialized plan actually costs 35). The same issue affects chained encoders
/// with exists semantics, where effects are chained through intermediate variables
/// but costs are still evaluated at x_t.
///
/// Therefore, SDAC + B&B is only sound with sequential, forall, or R2E semantics.
/// The combination of SDAC + B&B + exists-step is rejected at startup (see
/// sdac_unsafe() in strategy_spec.hpp).
/// For constant action costs, all semantics are correct since cost is independent
/// of the evaluation state.
///
/// Result status:
///   - SOLVED_OPTIMALLY: B&B proved no cheaper plan exists (UNSAT with cost bound).
///   - SOLVED_SATISFICING: a plan was found but optimality was not proven within
///     resource limits (timeout or max_steps exhausted). This happens when the
///     abstract suffix keeps producing SAT+abstract results at every horizon,
///     preventing the UNSAT proof needed for optimality.
///   - UNSOLVABLE_INCOMPLETELY: no plan found within limits.
class BranchAndBoundPlanner : public BasePlanner {
public:
    BranchAndBoundPlanner(const Problem& problem, BaseEncoder& encoder,
                          z3::context& ctx,
                          const InterferenceAnalysis* interference,
                          SemanticsKind semantics);

    Plan search() override;
    bool solution_found() const override { return solution_found_; }
    bool optimality_proven() const override { return optimality_proven_; }
    double best_cost() const override { return best_cost_; }

    void set_propagator_strategy(std::unique_ptr<PropagatorStrategy> propagator) override;
    std::string get_propagator_strategy_name() const override;
    z3::solver& get_solver() override { return solver_; }

    /// Set SDAC cost lower bounds (forwarded to AbstractSuffixEncoder).
    void set_cost_lower_bounds(std::vector<double> bounds) { abstract_.set_cost_lower_bounds(std::move(bounds)); }

private:
    const Problem& problem_;
    BaseEncoder& encoder_;
    z3::context& ctx_;
    z3::solver solver_;
    bool solution_found_ = false;
    bool optimality_proven_ = false;
    double best_cost_ = 0.0;
    std::unique_ptr<PropagatorStrategy> propagator_strategy_;

    AbstractSuffixEncoder abstract_;

    void add_timestep_constraints(int timestep);

    static double extract_cost_from_model(const z3::model& model, const z3::expr& cost_expr);
};

} // namespace rantanplan
