#pragma once

#include "../problem/problem.hpp"
#include "../grounding/interval.hpp"
#include <unordered_map>

namespace rantanplan {

/// Tighten -inf lower and +inf upper entries in a numeric bounds map
/// using syntactic precondition analysis of the grounded problem.
///
/// For each fluent with -inf lower bound: verify f >= 0 by checking that
/// every DECREASE effect is guarded by a precondition f >= delta (same ExprID).
///
/// For each fluent with +inf upper bound: verify f <= C by checking that
/// every INCREASE effect is guarded by a precondition f + delta <= C.
///
/// Returns the number of entries tightened.
int tighten_bounds_syntactically(
    std::unordered_map<ExprID, Interval>& bounds,
    const Problem& problem);

} // namespace rantanplan
