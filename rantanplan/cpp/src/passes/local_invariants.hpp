#pragma once

#include "../problem/expr_pool.hpp"
#include <string>
#include <vector>

namespace rantanplan {

/// A verified mutex group (at-most-one or exactly-one).
struct MutexConstraint {
    std::vector<ExprID> members;   // boolean fluent ExprIDs
    bool exactly_one = false;      // true = exactly-one, false = at-most-one
    std::string label;             // human-readable label for logging
};

/// A verified numeric bound on a single fluent.
struct BoundConstraint {
    ExprID fluent_id;
    double bound;
    bool is_lower;                 // true = fluent >= bound, false = fluent <= bound
};

/// A domain constraint on an object fluent: restricts to a finite set of values.
struct DomainConstraint {
    ExprID fluent_id;                      // ground object fluent
    std::vector<int> valid_object_indices; // reachable object values (sorted)
};

/// State constraints discovered by the invariant oracle.
/// These hold in every reachable state and are injected at every timestep.
struct StateConstraints {
    std::vector<MutexConstraint> mutexes;
    std::vector<BoundConstraint> bounds;
    std::vector<DomainConstraint> domains;

    bool empty() const { return mutexes.empty() && bounds.empty() && domains.empty(); }
};

} // namespace rantanplan
