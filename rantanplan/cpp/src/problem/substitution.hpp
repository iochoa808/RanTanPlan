#pragma once

#include <unordered_map>
#include <string>
#include "expr_pool.hpp"

namespace rantanplan {

/// A substitution maps parameter names (e.g., "?from", "from") to the ExprID
/// of the concrete object constant that replaces them.
///
/// Example: {"from" -> ExprID(city0_constant), "to" -> ExprID(city1_constant)}
using Substitution = std::unordered_map<std::string, ExprID>;

/// Recursively walk an ExprID tree and replace every VARIABLE node whose name
/// appears in `var_subst` with the corresponding ExprID. Used during forall-effect
/// expansion to bind quantified variables (VARIABLE nodes) to concrete constants.
/// Mirrors substitute() but targets VARIABLE nodes instead of PARAMETER nodes.
ExprID substitute_vars(ExprPool& pool, ExprID expr, const Substitution& var_subst);

/// Recursively walk an ExprID tree and replace every PARAMETER node whose name
/// appears in `subst` with the corresponding ExprID. The result is interned
/// into the pool, so structurally identical substitutions share the same ExprID.
///
/// This is the fundamental operation for grounding: it turns a lifted expression
/// like (at ?obj ?loc) into a ground expression like (at robot1 city0).
///
/// How it works:
///   - Leaf PARAMETER node whose payload string matches a key in subst
///     → return subst[name]
///   - Any other leaf node (CONSTANT, FLUENT_SYMBOL, etc.)
///     → return as-is (no children to recurse into)
///   - Internal node (AND, OR, STATE_VARIABLE, FUNCTION_APPLICATION, etc.)
///     → recurse on each child, then:
///       * if no child changed: return the original ExprID (identity optimization)
///       * if any child changed: intern a new ExprNode with the substituted
///         children. The ExprPool deduplicates automatically.
///
/// Performance: O(tree_size) per call. The ExprPool's interning ensures that
/// repeated substitutions producing identical trees share the same ExprID.
///
/// Thread safety: NOT thread-safe. The caller must ensure exclusive access to
/// the ExprPool during substitution (which is the normal case — the pool is
/// shared via shared_ptr but only mutated during problem construction or
/// grounding, never during solving).
ExprID substitute(ExprPool& pool, ExprID expr, const Substitution& subst);

} // namespace rantanplan
