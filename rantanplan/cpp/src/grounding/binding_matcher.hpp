#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "fact_index.hpp"
#include "../problem/problem.hpp"

namespace rantanplan {

/// A complete or partial binding maps action parameter indices to object indices.
///
/// Key:   parameter index (position in Action::parameters())
/// Value: object index    (position in Problem::objects())
///
/// Example for action move(?v - vehicle, ?from - loc, ?to - loc):
///   {0 -> 3, 1 -> 7, 2 -> 12}
///   means ?v=objects[3], ?from=objects[7], ?to=objects[12]
using PartialBinding = std::unordered_map<int, int>;

/// BindingMatcher finds all complete, type-consistent parameter bindings for a
/// lifted action such that every boolean precondition holds in the current
/// FactIndex.
///
/// THE KEY ALGORITHM — JOIN-BASED MATCHING
/// ========================================
///
/// Instead of trying all |Objects(type_1)| x ... x |Objects(type_k)| combinations
/// (which is what naive grounding does), we treat precondition matching like a
/// database join:
///
/// 1. EXTRACT ATOMS: From the action's precondition ExprID tree, extract all
///    boolean fluent applications (STATE_VARIABLE nodes). These are the "tables"
///    we join against. Both positive and negated atoms are extracted; positive
///    atoms drive the join phase, while negated atoms are applied as a
///    post-filter (since under delete-relaxation negated atoms are always
///    satisfiable during the join, but we still prune bindings where a negated
///    atom is actually present in the fact index).
///
/// 2. ORDER BY SELECTIVITY: Sort atoms so the most constraining one comes first.
///    "Most constraining" = fewest matching tuples in the FactIndex per new
///    parameter that would be bound. This heuristic minimizes the number of
///    intermediate partial bindings.
///
/// 3. EXTEND INCREMENTALLY: Start with an empty binding {}. For each atom,
///    look up matching tuples in the FactIndex and extend each existing partial
///    binding with consistent matches. Discard partial bindings that can't be
///    extended (early pruning).
///
/// 4. TYPE FILTER: When binding a parameter to an object, verify that the
///    object's type is a subtype of the parameter's declared type.
///
/// 5. NUMERIC/COMPLEX PRECONDITIONS: Anything that isn't a simple boolean
///    fluent atom (arithmetic comparisons, nested function applications) is
///    SKIPPED. We assume these are satisfiable — same over-approximation as
///    the delete-relaxation.
///
/// 6. UNBOUND PARAMETERS: If some parameters don't appear in any boolean
///    precondition atom (e.g., they only appear in numeric preconditions or
///    effects), we enumerate all type-compatible objects for those parameters
///    after the join phase. This is a Cartesian product over unbound params,
///    but only for those specific parameters — much smaller than the full
///    cross-product.
///
/// WHY THIS IS FAST
/// ================
///
/// For action move(?v, ?from, ?to) with 100 vehicles and 100 locations:
///   - Naive: tries 100 x 100 x 100 = 1,000,000 combinations
///   - Join: if only 5 vehicles are ever "at" anything, starts with 5 partial
///     bindings {?v, ?from}, extends by "road" facts for ?to.
///     Final count might be ~500 instead of 1,000,000.
class BindingMatcher {
public:
    explicit BindingMatcher(const Problem& problem, const FactIndex& facts);

    /// Find all complete parameter bindings for a lifted action such that
    /// all boolean preconditions are satisfiable in the current FactIndex.
    ///
    /// @param action  A lifted action (with non-empty parameters())
    /// @return Vector of complete bindings (param_index -> object_index maps).
    ///         Each represents one reachable ground instantiation.
    std::vector<PartialBinding> find_bindings(const Action& action) const;

private:
    const Problem& problem_;
    const FactIndex& facts_;

    // -----------------------------------------------------------------------
    // Internal: Precondition atom representation
    // -----------------------------------------------------------------------

    /// A single boolean fluent atom extracted from a precondition tree.
    ///
    /// For a precondition like and(at(?v, ?from), road(?from, ?to)):
    ///   Atom 1: fluent_schema_id = id_of("at"),
    ///           arg_param_index  = [0, 1]  (both args are action parameters)
    ///           arg_constant_obj = [-1, -1] (no constant arguments)
    ///   Atom 2: fluent_schema_id = id_of("road"),
    ///           arg_param_index  = [1, 2]
    ///           arg_constant_obj = [-1, -1]
    struct PrecondAtom {
        int fluent_schema_id;                // Index in Problem::fluents()
        std::vector<int> arg_param_index;    // For each fluent arg: which action
                                             //   parameter it is (-1 if constant)
        std::vector<int> arg_constant_obj;   // For each fluent arg: object index
                                             //   if constant (-1 if parameterized)
        bool negated;                        // Whether this atom was under NOT
    };

    /// An equality constraint between two action parameters or a parameter
    /// and a constant, extracted from (= ?x ?y) or (not (= ?x ?y)).
    struct EqualityConstraint {
        int param_a;        // First parameter index (-1 if constant)
        int param_b;        // Second parameter index (-1 if constant)
        int const_a;        // Object index if first arg is constant (-1 otherwise)
        int const_b;        // Object index if second arg is constant (-1 otherwise)
        bool negated;       // false => must be equal, true => must differ
    };

    /// Extract all boolean fluent atoms from a precondition ExprID tree.
    ///
    /// Walks the AND/OR/NOT/IMPLIES/IFF structure, collecting STATE_VARIABLE
    /// nodes whose head is a boolean fluent. Skips numeric sub-trees
    /// (comparisons, arithmetic).
    ///
    /// The `negated` flag tracks whether we're under an odd number of NOTs.
    void extract_atoms(ExprID expr_id,
                       const Action& action,
                       bool negated,
                       std::vector<PrecondAtom>& out_atoms,
                       std::vector<EqualityConstraint>& out_equalities) const;

    /// Order atoms by selectivity for the join: process the most constraining
    /// atom first (fewest matching tuples per new parameter bound).
    ///
    /// The score for an atom is:
    ///   num_matching_facts(atom.fluent_schema_id) / max(1, num_new_params(atom))
    ///
    /// Lower score = process first = more constraining.
    void order_by_selectivity(std::vector<PrecondAtom>& atoms) const;

    /// Extend a set of partial bindings with matches for one atom.
    ///
    /// For each existing partial binding, look at the atom's arguments:
    ///   - Already-bound parameter: the fact tuple must agree with the binding
    ///   - Unbound parameter: bind it to the fact tuple value (if type-compatible)
    ///   - Constant argument: the fact tuple must match the constant
    ///
    /// Returns the set of extended partial bindings. Bindings that can't be
    /// extended are dropped (this is the pruning that makes join-based matching
    /// fast).
    std::vector<PartialBinding> extend_bindings(
        const std::vector<PartialBinding>& current_bindings,
        const PrecondAtom& atom,
        const Action& action) const;

    /// Enumerate all type-compatible objects for unbound parameters and extend
    /// the given bindings with all combinations.
    ///
    /// This handles parameters that don't appear in any boolean precondition atom
    /// (e.g., parameters that are only in numeric preconditions or effects).
    std::vector<PartialBinding> fill_unbound_parameters(
        const std::vector<PartialBinding>& bindings,
        const Action& action) const;

    /// Filter bindings by equality constraints.
    /// Enforces (= ?x ?y) ⇒ same object, (not (= ?x ?y)) ⇒ different objects.
    std::vector<PartialBinding> filter_by_equality(
        const std::vector<PartialBinding>& bindings,
        const std::vector<EqualityConstraint>& equalities) const;

    /// Filter bindings by negated precondition atoms.
    /// Discards any binding where a negated boolean atom is actually present
    /// in the fact index (contradicts the negation).
    std::vector<PartialBinding> filter_by_negated_atoms(
        const std::vector<PartialBinding>& bindings,
        const std::vector<PrecondAtom>& negated_atoms) const;

    /// Check type consistency: is the object at index `obj_idx` in
    /// Problem::objects() compatible with the declared type of the action
    /// parameter at index `param_idx`?
    bool is_type_compatible(const Action& action, int param_idx, int obj_idx) const;

    /// Build a list of type-compatible object indices for each parameter.
    /// Cached per find_bindings() call (not across calls, since the problem
    /// doesn't change during one call anyway).
    std::vector<std::vector<int>> get_type_compatible_objects(const Action& action) const;
};

} // namespace rantanplan
