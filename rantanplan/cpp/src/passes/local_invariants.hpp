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

/// State constraints discovered by the invariant oracle.
/// These hold in every reachable state and are injected at every timestep.
struct StateConstraints {
    std::vector<MutexConstraint> mutexes;
    std::vector<BoundConstraint> bounds;

    bool empty() const { return mutexes.empty() && bounds.empty(); }
};

} // namespace rantanplan
