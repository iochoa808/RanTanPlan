#pragma once

#include "interval.hpp"
#include "numeric_bounds_index.hpp"
#include "../problem/expr_pool.hpp"
#include "../problem/problem.hpp"

namespace rantanplan {

/// Evaluate a ground numeric ExprID expression tree to an Interval,
/// using the NumericBoundsIndex for state variable lookups.
///
/// The ExprID is expected to be fully ground (no PARAMETER nodes).
/// Unknown/unsupported operations return Interval::unbounded() (safe).
Interval evaluate_interval(ExprID eid,
                           const ExprPool& pool,
                           const NumericBoundsIndex& bounds,
                           const Problem& problem);

/// Check if a ground precondition ExprID tree is satisfiable given the
/// current numeric bounds. Returns false only if provably unsatisfiable
/// (the action should be pruned).
///
/// Boolean atoms always return true (handled by the boolean FactIndex).
/// NOT nodes return true (can't safely negate interval checks).
/// Unknown structures return true (safe over-approximation).
bool numeric_precondition_satisfiable(ExprID eid,
                                       const ExprPool& pool,
                                       const NumericBoundsIndex& bounds,
                                       const Problem& problem);

} // namespace rantanplan
