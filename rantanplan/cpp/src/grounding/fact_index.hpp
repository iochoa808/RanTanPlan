#pragma once

#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cstddef>
#include <functional>
#include "problem.hpp"

namespace rantanplan {

/// FactIndex stores the set of ground boolean facts known to be reachable.
///
/// It serves two purposes in the grounder:
///
///   1. MEMBERSHIP TEST: "Is (at robot1 city0) reachable?" — used to check
///      whether an action's preconditions are satisfied under a given binding.
///
///   2. EXTENSION QUERY: "What object tuples make fluent F true?" — used by
///      the join-based matcher to enumerate candidate bindings for a precondition
///      atom like (at ?obj ?loc).
///
/// Facts are indexed by fluent schema ID (the index of the Fluent in
/// Problem::fluents()), so looking up all facts for a given fluent is O(1).
///
/// Inside each fluent's bucket, facts are stored as vectors of object indices
/// (position in Problem::objects()). For a fluent `at(obj, loc)`, a fact might
/// be stored as [3, 7] meaning object_at_index_3 at location_at_index_7.
///
/// Design note: We store object indices rather than ExprIDs because the join
/// matcher needs to compare and bind individual argument positions, not entire
/// expression trees. ExprIDs are used at the ExprPool level; here we work with
/// the integer-index level for speed.
class FactIndex {
public:
    /// Construct a FactIndex for the given problem.
    /// Does NOT automatically seed from the initial state — call
    /// initialize_from_initial_state() explicitly.
    explicit FactIndex(const Problem& problem);

    /// Seed the index with all boolean facts from the problem's initial state.
    ///
    /// For each initial assignment whose fluent is a boolean predicate with
    /// value `true`, we decompose the STATE_VARIABLE ExprID into
    /// (fluent_schema_id, [object_indices...]) and add it to the index.
    ///
    /// Numeric initial values are ignored — they don't constrain reachability
    /// in the delete-relaxation (any numeric condition is assumed satisfiable).
    void initialize_from_initial_state();

    /// Add a ground boolean fact. Returns true if the fact was new.
    ///
    /// @param fluent_schema_id  Index of the fluent in Problem::fluents()
    /// @param object_indices    One int per fluent parameter — the index in
    ///                          Problem::objects() for each argument.
    bool add_fact(int fluent_schema_id, const std::vector<int>& object_indices);

    /// Check if a specific ground fact is known to be reachable.
    bool contains(int fluent_schema_id, const std::vector<int>& object_indices) const;

    /// Get all known ground tuples for a given fluent schema.
    ///
    /// Returns a reference to the vector of known argument tuples.
    /// Each tuple is a vector of object indices (position in Problem::objects()).
    ///
    /// Example: for fluent "at" (schema id=2), might return:
    ///   {{0, 3}, {0, 5}, {1, 3}}
    /// meaning at(obj0, loc3), at(obj0, loc5), at(obj1, loc3).
    ///
    /// The returned reference remains valid until the next add_fact() call.
    const std::vector<std::vector<int>>& get_facts(int fluent_schema_id) const;

    /// Total number of known facts across all fluents.
    size_t total_fact_count() const { return total_facts_; }

    /// Number of boolean fluent schemas tracked.
    size_t fluent_schema_count() const { return facts_by_fluent_.size(); }

private:
    const Problem& problem_;
    size_t total_facts_ = 0;

    /// Primary storage: fluent_schema_id → list of ground argument tuples.
    /// Outer vector is indexed by fluent schema ID.
    /// Inner: each element is a tuple of object indices.
    std::vector<std::vector<std::vector<int>>> facts_by_fluent_;

    /// Fast membership check: fluent_schema_id → set of full tuples.
    /// Previous implementation used hash-only (uint64_t) which could silently
    /// drop distinct facts on collision, under-approximating reachability.
    /// Now stores the full tuple with a custom hasher so collisions are
    /// resolved by equality comparison.
    struct VectorHash {
        size_t operator()(const std::vector<int>& v) const {
            // FNV-1a mixing — fast for short tuples (typical arity 1-3).
            uint64_t h = 14695981039346656037ULL;
            for (int x : v) {
                h ^= static_cast<uint64_t>(static_cast<uint32_t>(x));
                h *= 1099511628211ULL;
            }
            return static_cast<size_t>(h);
        }
    };
    std::vector<std::unordered_set<std::vector<int>, VectorHash>> fact_sets_;

    /// An empty vector returned by get_facts() for out-of-range schema IDs.
    static const std::vector<std::vector<int>> empty_facts_;

    /// Given a STATE_VARIABLE ExprID, extract the fluent schema ID and the
    /// object indices for each argument.
    ///
    /// A STATE_VARIABLE in the ExprPool has the layout:
    ///   children[0] = FLUENT_SYMBOL (whose payload_string is the fluent name)
    ///   children[1..] = arguments (CONSTANT nodes whose payload_string is
    ///                    the object name)
    ///
    /// Returns false if the ExprID is not a valid boolean STATE_VARIABLE or if
    /// any argument can't be resolved to an object index.
    bool decompose_state_variable(ExprID eid,
                                  int& out_fluent_schema_id,
                                  std::vector<int>& out_object_indices) const;
};

} // namespace rantanplan
