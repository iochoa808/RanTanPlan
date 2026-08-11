#pragma once

#include "interval.hpp"
#include "../problem/problem.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rantanplan {

/// Index of interval bounds for ground numeric fluents.
///
/// Parallel to FactIndex (which tracks boolean facts), this tracks numeric
/// fluent bounds as closed intervals [lower, upper]. Used by the
/// ReachabilityGrounder to prune actions with unsatisfiable numeric
/// preconditions.
///
/// Key semantics:
///   - Uninitialized numeric fluents default to [0, 0] (UP convention).
///   - Updates use convex_union (monotonic widening under delete-relaxation).
///   - Per-fluent widening after a threshold guarantees termination.
class NumericBoundsIndex {
public:
    explicit NumericBoundsIndex(const Problem& problem);

    /// Seed from initial state: for each numeric fluent assignment, set bounds
    /// to [value, value]. Fluents not mentioned remain at default [0, 0].
    void initialize_from_initial_state();

    /// Get current bounds for a ground numeric fluent.
    /// Returns [0, 0] if the fluent has never been seen.
    Interval get_bounds(int fluent_schema_id,
                        const std::vector<int>& obj_indices) const;

    /// Update bounds via convex union. Returns true if bounds changed.
    /// Increments the expansion counter for widening.
    bool update_bounds(int fluent_schema_id,
                       const std::vector<int>& obj_indices,
                       const Interval& new_interval);

    /// If the expansion count for either side exceeds the threshold,
    /// snap that side to -inf or +inf (guarantees termination).
    /// Directional: only widens the side(s) that have actually been moving.
    /// Frozen sides (from precompute_freezes) are never widened.
    void maybe_widen(int fluent_schema_id,
                     const std::vector<int>& obj_indices,
                     int threshold = 3);

    /// Lifted pre-analysis: for each numeric fluent schema, determine if
    /// a finite ceiling (upper) or floor (lower) can be guaranteed from the
    /// effect structure.  Frozen sides are never widened by maybe_widen().
    void precompute_freezes();

    /// Seed bounds from declared bounded-integer fluent value types.
    /// Clamps initial-state bounds to [lo, hi], registers type intervals for
    /// update_bounds() clamping, and freezes both sides so maybe_widen() never
    /// exceeds the declared range.
    /// Call after initialize_from_initial_state() and precompute_freezes().
    void seed_from_type_bounds();

    /// Decompose a ground STATE_VARIABLE ExprID for a numeric fluent into
    /// (schema_id, obj_indices). Returns false if not a numeric fluent or
    /// if decomposition fails.
    bool decompose_numeric_state_variable(ExprID eid,
                                          int& out_schema_id,
                                          std::vector<int>& out_obj_indices) const;

private:
    const Problem& problem_;

    /// Compact key for a ground numeric fluent.
    struct GroundKey {
        int schema_id;
        std::vector<int> obj_indices;

        bool operator==(const GroundKey& o) const {
            return schema_id == o.schema_id && obj_indices == o.obj_indices;
        }
    };

    /// FNV-1a hash for GroundKey.
    struct GroundKeyHash {
        size_t operator()(const GroundKey& k) const {
            uint64_t h = 14695981039346656037ULL;
            h ^= static_cast<uint64_t>(static_cast<uint32_t>(k.schema_id));
            h *= 1099511628211ULL;
            for (int v : k.obj_indices) {
                h ^= static_cast<uint64_t>(static_cast<uint32_t>(v));
                h *= 1099511628211ULL;
            }
            return static_cast<size_t>(h);
        }
    };

    std::unordered_map<GroundKey, Interval, GroundKeyHash> bounds_;
    std::unordered_map<GroundKey, int, GroundKeyHash> lower_expansion_count_;
    std::unordered_map<GroundKey, int, GroundKeyHash> upper_expansion_count_;

    /// Fluent schema IDs whose upper/lower bound should never be widened.
    /// Populated by precompute_freezes().
    std::unordered_set<int> freeze_upper_;
    std::unordered_set<int> freeze_lower_;

    /// Per-schema type bounds from bounded-int value-type declarations.
    /// update_bounds() clamps merged intervals to these when present.
    std::unordered_map<int, Interval> type_bounds_per_schema_;
};

} // namespace rantanplan
