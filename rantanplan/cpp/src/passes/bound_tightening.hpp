#pragma once

#include "../problem/problem.hpp"
#include "../grounding/interval.hpp"
#include "local_invariants.hpp"
#include <unordered_map>
#include <vector>

namespace rantanplan {

/// Tighten -inf lower and +inf upper entries in a numeric bounds map
/// using syntactic precondition analysis of the grounded problem.
///
/// Returns the number of entries tightened.
int tighten_bounds_syntactically(
    std::unordered_map<ExprID, Interval>& bounds,
    const Problem& problem);

/// Discover conservation laws (f + g == C) from syntactic analysis of
/// action effects. A pair (f, g) is a conservation law if every action
/// that modifies either fluent has constant, unconditional, non-ASSIGN
/// effects where delta_f + delta_g == 0.
std::vector<ConservationConstraint> discover_conservation_laws(
    const Problem& problem);

/// Derive tighter bounds from conservation laws. If f + g == C and
/// g >= lower_g, then f <= C - lower_g (and symmetrically).
void derive_bounds_from_conservations(
    std::unordered_map<ExprID, Interval>& bounds,
    const std::vector<ConservationConstraint>& conservations);

} // namespace rantanplan
