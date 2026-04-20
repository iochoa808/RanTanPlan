#pragma once

#include "pass.hpp"
#include "../analysis/numeric_relaxed_planning_graph.hpp"
#include <vector>
#include <unordered_set>

namespace rantanplan {

// ============================================================================
// InvariantOraclePass
// ============================================================================
//
// Discovers state constraints (properties that hold in every reachable state)
// from structural analysis of the grounded problem. These are injected into
// the SMT solver at every timestep to prune infeasible assignments.
//
// Two categories of constraints are produced:
//
// --- Mutex Groups (Tier 1 Syntactic Check) ---
//
// For boolean predicates with arity >= 2, we group ground instances by each
// argument position. For example, `located(plane1, city0..city5)` forms a
// group of 6 members sharing the entity `plane1`.
//
// A group is AT-MOST-ONE if no reachable state can have two members TRUE
// simultaneously. The syntactic check verifies this WITHOUT any SMT call
// by inspecting action effects:
//
//   Sufficient conditions for at-most-one:
//   1. No action has conditional effects on any group member.
//   2. Every action that sets a member TRUE also sets exactly one member FALSE.
//   3. No action sets more than one member TRUE simultaneously.
//
// These conditions guarantee the group cardinality never increases beyond 1.
// If the initial state has at most one TRUE member, the invariant holds.
// This is the same pattern Fast Downward's translator uses for SAS+ variables:
// movement actions like `drive(truck, from, to)` delete `located(truck, from)`
// and add `located(truck, to)` — a delete-add pair.
//
// A group is upgraded to EXACTLY-ONE if additionally:
//   - The initial state has exactly one TRUE member.
//   - Every action that deletes a member also adds one (cardinality never
//     decreases).
//
// Exactly-one groups give Z3 the equivalent of finite-domain variables
// (z3::atmost + z3::atleast). This is the primary driver of solver speedups.
//
// --- RPG Fixpoint Bounds ---
//
// For each numeric fluent, the Numeric Relaxed Planning Graph computes an
// over-approximate interval [lower, upper] of reachable values. When these
// are finite, we inject them as `fluent >= lower` and `fluent <= upper` at
// every timestep. This prevents the solver from exploring states with
// out-of-bounds numeric values (e.g., negative fuel, stock exceeding supply).
//
// ============================================================================

/**
 * @brief Preprocessing pass that discovers state constraints.
 *
 * Phase 1 (no SMT):
 *   - Injects RPG fixpoint bounds as permanent constraints.
 *   - Discovers mutex groups via Tier 1 syntactic check (delete-add pattern).
 *
 * Results are stored on PipelineResult::state_constraints for the encoder
 * to inject at every timestep.
 */
class InvariantOraclePass : public Pass {
public:
    void apply(PipelineResult& result) const override;
    std::string name() const override { return "InvariantOracle"; }

private:
    /// Collect RPG fixpoint bounds (finite only).
    std::vector<BoundConstraint> collect_rpg_bounds(
        const NumericRelaxedPlanningGraph& rpg,
        const Problem& problem) const;

    /// Generate mutex candidates by grouping boolean fluent instances
    /// by predicate and argument position.
    struct MutexCandidate {
        std::vector<ExprID> members;
        std::string entity;
        std::string predicate;
    };
    std::vector<MutexCandidate> generate_mutex_candidates(const Problem& problem) const;

    /// Tier 1 syntactic check: verify at-most-one by checking that every action
    /// that adds a member also deletes exactly one member, with no conditional effects.
    /// Returns true if at-most-one is verified.
    bool syntactic_mutex_check(const MutexCandidate& candidate,
                               const Problem& problem) const;

    /// Check if the group is exactly-one (syntactic): initial state has exactly
    /// one TRUE member, and every action that deletes a member also adds one.
    bool syntactic_exactly_one_check(const MutexCandidate& candidate,
                                     const Problem& problem,
                                     const std::unordered_set<ExprID>& initially_true) const;
};

} // namespace rantanplan
