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
    if (t->is_set()) {
        const Type* element = detail::find_element_type(t->set_element_type_name(), problem);
        if (!element) return result;

        if (const Type* bounded = element->bounded_int_ancestor()) {
            // Bounded int elements: the domain is the closed interval [lo, hi].
            for (int64_t v = bounded->lower_bound(); v <= bounded->upper_bound(); ++v) {
                result.push_back({v});
            }
        } else {
            // Object elements: the domain is every object of that type, by object index.
            for (int idx : problem.objects_of_type(element)) {
                result.push_back({static_cast<int64_t>(idx)});
            }
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

    if (t->is_array()) {
        const Type* inner = detail::find_element_type(t->array_element_type_name(), problem);
        // Keep descending while the element type is itself an array; anything else
        // (bool, bounded int, object, set) is the leaf. May be nullptr if unresolved.
        if (inner && inner->is_array()) return leaf_element_type(inner, problem);
        return inner;
    }

    return nullptr;
}

} // namespace rantanplan
