#pragma once

#include "binding_matcher.hpp"   // PartialBinding
#include "../problem/problem.hpp"
#include "../problem/action.hpp"

namespace rantanplan {

/// Instantiate a lifted action with a complete parameter binding, producing
/// a ground action (zero parameters).
///
/// Every PARAMETER node in the precondition and effects is replaced by the
/// CONSTANT node for the bound object.
///
/// The ground action's name follows the convention:
///   "move(city0, city1, plane0)"
///
/// @param pool           Mutable expression pool (new ground expressions are interned)
/// @param problem        The problem providing objects/types
/// @param lifted_action  The lifted (parameterised) action to instantiate
/// @param binding        Complete mapping: param index → object index
/// @return A ground Action (parameters().empty() == true)
Action instantiate_action(ExprPool& pool,
                          const Problem& problem,
                          const Action& lifted_action,
                          const PartialBinding& binding);

} // namespace rantanplan
