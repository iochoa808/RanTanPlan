#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <functional>
#include "fact_index.hpp"

namespace rantanplan {

/// ObjectFluentIndex tracks the set of reachable values for ground object fluent
/// instances under delete-relaxation semantics.
///
/// Object fluents map a tuple of objects to an object value, e.g.:
///   (location rover0) = waypoint3
///   (aircraft-at plane1) = city0
///
/// Under delete-relaxation, values ACCUMULATE monotonically: once a value is
/// reachable for a fluent instance, it stays reachable even after ASSIGN effects
/// add new values. The index stores a set of reachable values per ground instance.
///
/// The index serves the BindingMatcher via "extended tuples" — for a fluent with
/// arity N, each extended tuple has N+1 columns: the N argument object indices
/// plus one value object index. This allows the join-based matching to constrain
/// both fluent arguments and the equality target in a single join step.
///
/// Example: fluent `location(?r - rover) - waypoint` with known values:
///   location(rover0) = {wp0, wp1}
///   location(rover1) = {wp2}
/// Extended tuples for schema_id of "location":
///   [[rover0_idx, wp0_idx], [rover0_idx, wp1_idx], [rover1_idx, wp2_idx]]
///
/// A precondition `(= (location ?r) ?p)` becomes a PrecondAtom with
/// arg_param_index = [idx_of_?r, idx_of_?p], joining against these tuples.
class ObjectFluentIndex {
public:
    explicit ObjectFluentIndex(const Problem& problem);

    /// Seed from initial state: for each assignment where the fluent is an
    /// object fluent and the value is a CONSTANT (object name), add the value.
    void initialize_from_initial_state();

    /// Add a reachable value for a ground object fluent instance.
    /// Returns true if the value was new (not previously known).
    bool add_value(int fluent_schema_id,
                   const std::vector<int>& arg_obj_indices,
                   int value_obj_index);

    /// Get all extended tuples (args... + value) for join-based matching.
    /// Each tuple has arity = fluent_parameter_count + 1.
    const std::vector<std::vector<int>>& get_extended_tuples(int fluent_schema_id) const;

    /// Get reachable value set for a specific ground fluent instance.
    /// Used by Pattern C post-filter to check value set intersection.
    const std::unordered_set<int>& get_values(
        int fluent_schema_id,
        const std::vector<int>& arg_obj_indices) const;

    /// Total number of (fluent_instance, value) pairs tracked.
    size_t total_entry_count() const { return total_entries_; }

    /// Number of fluent schemas tracked.
    size_t fluent_schema_count() const { return by_fluent_.size(); }

private:
    const Problem& problem_;

    // FNV-1a hash for vector<int> — same pattern as FactIndex.
    struct VectorHash {
        size_t operator()(const std::vector<int>& v) const {
            uint64_t h = 14695981039346656037ULL;
            for (int x : v) {
                h ^= static_cast<uint64_t>(static_cast<uint32_t>(x));
                h *= 1099511628211ULL;
            }
            return static_cast<size_t>(h);
        }
    };

    // Per fluent schema: maps arg_tuple -> set of reachable value object indices.
    struct ArgTupleMap {
        std::unordered_map<std::vector<int>,
                           std::unordered_set<int>, VectorHash> values;
    };
    std::vector<ArgTupleMap> by_fluent_;  // indexed by fluent schema ID

    // For join-based matching: schema_id -> list of extended tuples.
    // Each tuple = [arg0, ..., argN, value_obj_index].
    // Appended incrementally on add_value().
    std::vector<std::vector<std::vector<int>>> extended_tuples_;

    size_t total_entries_ = 0;

    static const std::vector<std::vector<int>> empty_tuples_;
    static const std::unordered_set<int> empty_values_;

    /// Decompose a STATE_VARIABLE ExprID for an object fluent.
    /// Returns false if not an object fluent or decomposition fails.
    bool decompose_object_fluent(ExprID eid,
                                  int& out_schema_id,
                                  std::vector<int>& out_arg_indices) const;

    /// Resolve a value ExprID to an object index.
    /// The value must be a CONSTANT with a string payload (object name).
    bool resolve_value_object(ExprID value_eid, int& out_obj_index) const;
};

} // namespace rantanplan
