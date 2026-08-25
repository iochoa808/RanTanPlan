#pragma once

// [XTS-UnFun] Shared helpers for the UF array/set encoding (--array-encoding uf).
//
// In UF mode arrays and sets are modelled as uninterpreted functions, so there is no
// Z3 Array-theory extensionality to lean on: every frame axiom, every whole-array /
// whole-set ASSIGN, and every equality between two array/set values has to be asserted
// *pointwise*, one assertion per cell or per candidate element.
//
// That enumeration is always finite, never a real quantifier:
//   - PDDL array sizes are fixed at declaration time, and
//   - set element types are always bounded-int or object typed
// (the same assumption docs/z3_encoding_rationale.md §7-10 relies on for Theory-mode
// set operations).
//
// Both grounded_encoder.cpp (frame axioms, initial state, per-cell bounds) and
// grounded_encoding_visitor.cpp (whole-array/set EQUALS) include this header, so the
// two call sites cannot drift apart on what "the domain of this fluent" means.

#include "../problem/problem.hpp"
#include "../problem/type.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rantanplan {
namespace detail {

// Look up a type by the name stored on an array/set declaration.
//
// Types coming from the unified-planning front end are registered under a "up:"
// namespace prefix ("up:int", "up:integer[0,2]", ...) while natively parsed PDDL types
// are not, and an element type name may have been recorded either way. Try the name
// verbatim first, then the prefixed spelling. Returns nullptr if neither exists.
inline const Type* find_element_type(const std::string& name, const Problem& problem) {
    if (const Type* t = problem.find_type(name)) return t;
    return problem.find_type("up:" + name);
}

} // namespace detail

// [XTS-UnFun] Enumerate the finite candidate-element domain of a SET's element type as
// raw index values.
//
// This is the single place that decides what "every possible element of this set" means:
// a bounded-int element type contributes its closed interval [lo, hi]; anything else is
// object-typed and contributes one entry per object of that type, by object index.
// Every set-shaped enumeration in the encoder funnels through here — the whole-set
// operations in grounded_encoding_visitor.cpp (subset/disjoint/cardinality/set algebra)
// as well as enumerate_array_domain below — so a change to that rule lands everywhere at
// once instead of in five hand-rolled copies.
//
//   element = integer[0,2]  (from set{up:integer[0,2]})  ->  {0, 1, 2}
//   element = card          (an object type)             ->  {obj_idx_0, obj_idx_1, ...}
//   element = nullptr       (unresolved)                 ->  {}
inline std::vector<int64_t> enumerate_set_element_domain(const Type* element,
                                                         const Problem& problem) {
    std::vector<int64_t> result;
    if (!element) return result;

    if (const Type* bounded = element->bounded_int_ancestor()) {
        // Bounded int elements: the domain is the closed interval [lo, hi].
        for (int64_t v = bounded->lower_bound(); v <= bounded->upper_bound(); ++v) {
            result.push_back(v);
        }
    } else {
        // Object elements: the domain is every object of that type, by object index.
        for (int idx : problem.objects_of_type(element)) {
            result.push_back(static_cast<int64_t>(idx));
        }
    }
    return result;
}

// [XTS-UnFun] Enumerate every concrete index tuple in an array fluent's static shape
// (cartesian product across nesting levels, outermost dimension first), or every
// element index in a set fluent's finite domain (as a length-1 tuple).
//   array[4, up:int]                 -> {{0},{1},{2},{3}}
//   array[2, array[2, up:int]]       -> {{0,0},{0,1},{1,0},{1,1}}
//   set{up:integer[0,2]}             -> {{0},{1},{2}}
//   set{card}  (card: object type)   -> {{obj_idx_0},{obj_idx_1},...}
//
// Returns an empty vector for anything that is not an array or set type, or whose
// size / domain cannot be determined. That is purely defensive: callers should already
// know `t` is array/set-typed (via array_fluent_ids_) before calling this.
inline std::vector<std::vector<int64_t>> enumerate_array_domain(const Type* t, const Problem& problem) {
    std::vector<std::vector<int64_t>> result;
    if (!t) return result;

    // --- Sets: one length-1 tuple per candidate element of the element type. ---
    // Delegates the "what counts as a candidate element" rule to
    // enumerate_set_element_domain above; this level only wraps each value as a
    // length-1 index tuple so sets and arrays share one return shape.
    if (t->is_set()) {
        const Type* element = detail::find_element_type(t->set_element_type_name(), problem);
        for (int64_t v : enumerate_set_element_domain(element, problem)) {
            result.push_back({v});
        }
        return result;
    }

    // --- Arrays: index tuples over this dimension crossed with the nested ones. ---
    if (t->is_array()) {
        const int64_t size = t->array_size();
        if (size <= 0) return result;

        const Type* inner = detail::find_element_type(t->array_element_type_name(), problem);

        if (inner && inner->is_array()) {
            // Nested array: enumerate the remaining dimensions once, then prefix each
            // of those tuples with this level's index i. Note we stop recursing at an
            // array-of-sets leaf — the set stays a single cell here, it is not expanded
            // into its own dimension.
            const auto sub = enumerate_array_domain(inner, problem);
            result.reserve(static_cast<size_t>(size) * sub.size());
            for (int64_t i = 0; i < size; ++i) {
                for (const auto& rest : sub) {
                    std::vector<int64_t> tuple;
                    tuple.reserve(rest.size() + 1);
                    tuple.push_back(i);
                    tuple.insert(tuple.end(), rest.begin(), rest.end());
                    result.push_back(std::move(tuple));
                }
            }
        } else {
            // Innermost (or only) dimension: one length-1 tuple per cell.
            result.reserve(static_cast<size_t>(size));
            for (int64_t i = 0; i < size; ++i) {
                result.push_back({i});
            }
        }
        return result;
    }

    // Not an array or set type: nothing to enumerate.
    return result;
}

// [XTS-UnFun] Walk down an array type's nesting levels, reporting both how many levels
// were descended and the type sitting at the bottom.
//
// This is the single array-descent loop in the encoder — "follow array_element_type_name()
// while the element type is itself an array". Its three consumers each want a different
// half of the answer, which is why it returns the pair rather than just a Type*:
//   - leaf_element_type below wants only the leaf;
//   - Z3VariableFactory::resolve_uf_shape wants the level count (that IS the UF function's
//     arity for an array fluent) *and* the leaf, and used to hand-roll the walk twice over,
//     once to count and once via resolve_elem_sort_at_depth to resolve;
//   - Z3VariableFactory::resolve_elem_sort_at_depth stops after a caller-supplied number of
//     levels instead of running to the leaf, hence `max_levels`.
//
//   array[2, array[2, up:int]]           ->  {2, up:int}
//   array[4, set{card}]                  ->  {1, set{card}}  (a set leaf is not a level)
//   up:int  (not an array at all)        ->  {0, up:int}
//   array[2, array[2, up:int]], levels=1 ->  {1, array[2, up:int]}
//
// `max_levels` < 0 means "descend all the way to the leaf". `problem` is nullable so that
// callers holding an optional Problem (Z3VariableFactory's problem_, unset until
// set_problem()) need no guard of their own: without a Problem no element type name can be
// resolved, so the descent stops after one level with a null leaf, exactly as the
// hand-rolled walks did.
inline std::pair<unsigned, const Type*> descend_array_levels(const Type* t,
                                                             const Problem* problem,
                                                             int max_levels = -1) {
    unsigned levels = 0;
    const Type* cur = t;
    while (cur && cur->is_array() &&
           (max_levels < 0 || levels < static_cast<unsigned>(max_levels))) {
        ++levels;
        cur = problem ? detail::find_element_type(cur->array_element_type_name(), *problem)
                      : nullptr;
    }
    return {levels, cur};
}

// [XTS-UnFun] Return the leaf element Type* that enumerate_array_domain's recursion
// bottoms out at for a given array/set fluent type — the same descent through nested
// array dimensions, but yielding the type instead of the enumerated index values.
//
// UF-mode default-init and per-cell bounds use it to decide what a "default" or
// "bounded" leaf value looks like, by testing the result for nullptr / is_bool() /
// is_set() (the array-of-sets leaf case) / bounded_int_ancestor().
inline const Type* leaf_element_type(const Type* t, const Problem& problem) {
    if (!t) return nullptr;

    // A set is already a leaf: its element type is what a cell holds.
    if (t->is_set()) {
        return detail::find_element_type(t->set_element_type_name(), problem);
    }

    // Neither array nor set: there is no element type to descend to. (descend_array_levels
    // would hand back `t` itself here, which is not what "leaf element" means.)
    if (!t->is_array()) return nullptr;

    // Anything that is not itself an array — bool, bounded int, object, set — is the leaf.
    // May be nullptr if some element type name along the way did not resolve.
    return descend_array_levels(t, &problem).second;
}

} // namespace rantanplan
