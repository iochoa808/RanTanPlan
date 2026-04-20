#pragma once

#include "../problem/problem.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace rantanplan {

// ============================================================================
// H2MutexChecker — Pairwise Mutex Detection via h² Reachability
// ============================================================================
//
// Determines which pairs of boolean facts can simultaneously be true in some
// reachable state. Pairs that are NOT co-reachable at fixpoint are mutex.
//
// === Algorithm ===
//
// We maintain two sets, grown monotonically until fixpoint:
//
//   reachable[f]       = fact f can be TRUE in some reachable state
//   co_reachable[f][g] = facts f and g can BOTH be TRUE in some reachable state
//
// Initialization:
//   reachable[f] = true   iff f is TRUE in the initial state
//   co_reachable[f][g] = true   iff BOTH f and g are TRUE in the initial state
//
// Fixpoint iteration — for each action a:
//   1. Check h²-applicability: all positive boolean preconditions of a must be
//      reachable, AND all PAIRS of preconditions must be co-reachable.
//      (If any precondition pair is mutex, a can never fire.)
//
//   2. For each add effect f of a (unconditional or conditional):
//      Mark reachable[f] = true.
//
//   3. For each pair (f, g) where f is an add effect of a:
//      Mark co_reachable[f][g] = true IF:
//        (a) g is ALSO an add effect of a (both produced simultaneously), OR
//        (b) g is already reachable, NOT unconditionally deleted by a, AND
//            g is co-reachable with every precondition of a.
//            (g "persists": it was true before a fired and a doesn't delete it.)
//
// At fixpoint:
//   mutex(f, g) = reachable[f] AND reachable[g] AND NOT co_reachable[f][g]
//
// === Soundness ===
//
// co_reachable is an OVER-approximation of truly simultaneously-achievable
// pairs. Every pair marked co-reachable is genuinely achievable OR we were
// too generous. Therefore, pairs NOT marked are provably mutex — no false
// mutex claims possible.
//
// Conservative choices that weaken detection but preserve soundness:
//   - Numeric preconditions are ignored (treated as always satisfiable).
//   - Conditional add effects are treated as always firing (might not).
//   - Only unconditional deletes count as definite (conditional deletes are
//     ignored — fact assumed to persist). This means more pairs pass the
//     persistence check, giving more co-reachable pairs.
//   - Negative preconditions (NOT f) are ignored.
//   - Disjunctive preconditions (OR) are ignored (treated as satisfiable).
//
// === Complexity ===
//
// Space: O(|B|²) where |B| = number of boolean grounded fluents.
//        For typical instances (50-200 booleans): 5-40KB.
//
// Time:  O(iterations × |A| × |B|²) worst case.
//        In practice: 3-5 iterations, sub-second for our benchmark scale.
//
// === Comparison with Tier 1 (syntactic check) ===
//
// Tier 1 only checks action effect STRUCTURE (delete-add pairs within a
// single action). It cannot reason about:
//   - Objects moving through intermediate predicates (board/debark pattern)
//   - Actions whose preconditions prevent co-achievement
//
// h² reasons about precondition interactions across the whole action set,
// catching "transit" patterns like:
//   located(person, city) --board--> in(person, aircraft) --debark--> located(person, city')
// Here, debark ADDS located(person, city') but Tier 1 fails because no single
// action both adds and deletes within the located group. h² proves the pair
// is mutex by showing debark's precondition in(person, ?) is itself mutex
// with located(person, city) — established at a prior fixpoint iteration.
//
// ============================================================================

class H2MutexChecker {
public:
    explicit H2MutexChecker(const Problem& problem);

    /// Run the h² fixpoint computation. Call once, then query results.
    void compute();

    /// Returns true if facts f and g are proven mutex (cannot both be true).
    /// Both must be boolean grounded fluents.
    bool is_mutex(ExprID f, ExprID g) const;

    /// Check if ALL pairs in a group are mutex (= at-most-one).
    bool all_pairs_mutex(const std::vector<ExprID>& group) const;

    /// Number of boolean facts tracked.
    size_t fact_count() const { return num_facts_; }

    /// Number of mutex pairs discovered.
    size_t mutex_pair_count() const;

private:
    const Problem& problem_;
    size_t num_facts_ = 0;

    // Compact index: ExprID -> [0, num_facts_)
    std::unordered_map<ExprID, size_t> fact_index_;
    std::vector<ExprID> index_to_fact_;  // reverse mapping

    // Reachability data (grown monotonically during fixpoint)
    std::vector<bool> reachable_;                     // [num_facts_]
    std::vector<std::vector<bool>> co_reachable_;     // [num_facts_][num_facts_] symmetric

    // Pre-processed action data for efficient iteration
    struct ActionData {
        std::vector<size_t> preconditions;  // positive boolean precondition indices
        std::vector<size_t> add_effects;    // indices of facts added (unconditional + conditional)
        std::vector<size_t> del_effects;    // indices of facts unconditionally deleted
    };
    std::vector<ActionData> actions_;

    // Extract positive boolean preconditions from an ExprID (AND-tree walk).
    void collect_positive_preconditions(ExprID eid, std::vector<size_t>& out) const;

    // Mark co_reachable symmetrically.
    bool mark_co_reachable(size_t i, size_t j);
};

} // namespace rantanplan
