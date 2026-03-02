#pragma once

#include "fact_index.hpp"
#include "binding_matcher.hpp"
#include "../problem/problem.hpp"

namespace rantanplan {

/// Result of the reachability grounding pass.
struct GroundingResult {
    /// The grounded problem (all actions are ground, no parameters).
    Problem grounded_problem;

    /// Whether the goal is provably unreachable (no ground actions can lead
    /// to any goal fact). Note: this is a necessary but not sufficient
    /// condition — the planner may still find the problem unsolvable.
    bool proven_unsolvable = false;

    /// Lower bound on plan length (from the fixpoint iteration depth).
    int lower_bound = 0;

    /// Number of fixpoint iterations (= layers in the relaxed planning graph).
    int iterations = 0;

    /// Number of ground actions produced.
    size_t ground_action_count = 0;
};

/// Reachability grounder using delete-relaxation fixpoint analysis.
///
/// This is the main grounder entry point. It implements:
///
///   1. Initialize FactIndex from the initial state (boolean facts).
///   2. Loop until fixpoint:
///      a. For each lifted action schema, use BindingMatcher to find all
///         parameter bindings whose positive boolean preconditions are
///         satisfied by the current FactIndex.
///      b. For each new binding, use instantiate_action() to create a
///         ground action.
///      c. Collect the add-effects of each new ground action and add them
///         to the FactIndex.
///      d. If no new facts were added, we've reached a fixpoint — stop.
///   3. Check goal reachability: verify that all positive boolean goal
///      atoms are in the final FactIndex.
///   4. Build a new Problem with only the reachable ground actions.
///
/// KEY DESIGN DECISIONS:
///
/// - We track "new bindings per action schema" to avoid re-instantiating
///   the same ground action across iterations. A binding that was valid in
///   iteration k is still valid in iteration k+1 (monotonicity).
///
/// - Numeric preconditions are SKIPPED (assumed satisfiable). This is the
///   standard delete-relaxation over-approximation.
///
/// - Numeric effects are SKIPPED for FactIndex updates (only boolean
///   add-effects produce new facts). Numeric fluents don't constrain
///   reachability in the boolean relaxation.
///
/// - The lower bound equals the iteration count: the earliest layer at which
///   all goal facts are present.
class ReachabilityGrounder {
public:
    explicit ReachabilityGrounder(const Problem& lifted_problem);

    /// Run the grounding fixpoint and return a GroundingResult.
    GroundingResult ground();

private:
    const Problem& lifted_problem_;

    /// Collect boolean add-effects from a ground action and insert them into
    /// the FactIndex. Returns the number of NEW facts added.
    size_t collect_add_effects(const Action& ground_action,
                               FactIndex& facts) const;

    /// Check if all positive boolean goal atoms are in the FactIndex.
    bool goals_reachable(const FactIndex& facts) const;
};

} // namespace rantanplan
