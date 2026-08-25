#pragma once

#include "../problem/problem.hpp"
#include "../problem/object.hpp"
#include "../problem/expr_pool.hpp"
#include "z3_variable_factory.hpp"
#include <z3++.h>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <stdexcept>

namespace rantanplan {

// Forward declarations
class Fluent;

/**
 * @brief Converts ExprID expressions to Z3 formulas using grounded variables
 *
 * Creates individual Z3 variables for each grounded fluent at specific timesteps.
 * Works in conjunction with GroundedEncoder to maintain consistency in variable
 * naming and creation.
 *
 * Key features:
 * - Fluent applications become individual variables (e.g., "located_plane1_city0_5")
 * - Proper type support for Boolean, Integer, Real, and Object fluents
 * - Uses Z3VariableFactory for consistent variable creation and naming
 * - Supports temporal encoding through timestep parameters
 *
 * All conversion methods throw std::runtime_error on malformed expressions.
 */
class GroundedEncodingVisitor {
private:
    z3::context& ctx_;
    const Problem* problem_;
    int current_timestep_;
    Z3VariableFactory* variable_factory_;

public:
    // Constructor
    GroundedEncodingVisitor(z3::context& ctx, const Problem* problem,
                           Z3VariableFactory* factory);

    // Temporal encoding methods
    void set_timestep(int timestep) { current_timestep_ = timestep; }
    int get_timestep() const { return current_timestep_; }
    void clear_timestep() { current_timestep_ = -1; }

    // ExprID-based conversion: walks ExprNode directly via ExprPool, no Expression needed
    z3::expr convert_from_pool(ExprID id, int timestep = -1);

    // [XTS-UnFun] Build the grounded Fluent identity (name + concrete parameter values)
    // for a plain STATE_VARIABLE ExprID. Factored out of convert_node's generic
    // STATE_VARIABLE branch so the UF read/write/frame-axiom code in GroundedEncoder
    // (which needs a Fluent to name its uninterpreted function via
    // Z3VariableFactory::get_array_uf, not a Z3 value) can reuse the exact same
    // construction. Public because GroundedEncoder calls it directly.
    Fluent build_grounded_fluent(ExprID sv_id) const;

    // [XTS-UnFun] Evaluate a set/array-valued expression at one concrete index tuple,
    // without ever materializing an intermediate Z3 Array-sorted term for it (except
    // as a last-resort fallback for expression shapes not otherwise recognized — see
    // .cpp). Used by the UF frame axiom (GroundedEncoder::encode_frames) to resolve
    // whole-array/set ASSIGN effects ("bag := <expr>") and by whole-array/set EQUALS,
    // pointwise, one cell at a time. `cell` is outermost-first raw index values,
    // matching enumerate_array_domain's tuple order and ArrayWriteRecord::indices.
    // Public for GroundedEncoder's use.
    z3::expr convert_array_cell_value(ExprID val_id, const std::vector<int64_t>& cell,
                                       int timestep);

    // [XTS-UnFun] Resolve the uninterpreted function standing in for array/set state
    // variable `sv_id`'s contents at `timestep`.
    //
    // Collapses the three-step preamble that every UF call site was repeating by hand:
    //   1. resolve the function's shape (arity + element sort) from the value type,
    //   2. rebuild the grounded Fluent identity that names the function,
    //   3. look the function up in Z3VariableFactory's per-timestep cache.
    // Step 1 had two spellings in the wild — derive the arity from the type, or take it
    // from however many indices the caller holds — and picking the wrong one silently
    // produces a function of the wrong arity. Both are available here explicitly:
    //
    //   uf_for(sv, vt, t)        arity from the type   (whole-fluent: init, frames,
    //                                                   whole-array assign)
    //   uf_for(sv, vt, t, n)     arity = n             (a read/write holding n indices)
    //
    // `vt` is passed in rather than looked up because callers disagree on the source —
    // GroundedEncoder uses Problem::type_for_id, the visitor uses the grounded Fluent's
    // own value_type() — and they are not interchangeable in every case.
    //
    //   board : array[3, array[3, int]]
    //     uf_for(board_sv, vt, 5)     -> read_board_5   : Int, Int -> Int
    //     uf_for(board_sv, vt, 5, 1)  -> read_board_5   : Int      -> Array(Int,Int)
    //   bag : set{card}
    //     uf_for(bag_sv, vt, 5)       -> read_bag_5     : Int      -> Bool
    //
    // The Fluent overload is for callers that already built the grounded identity (and
    // usually took `vt` off it): build_grounded_fluent does a linear fluent scan, and
    // convert_membership sits on a per-element path where re-deriving it would show up.
    const z3::func_decl& uf_for(const Fluent& fl, const Type* vt, int timestep, int arity = -1);
    const z3::func_decl& uf_for(ExprID sv_id, const Type* vt, int timestep, int arity = -1);

private:
    // [XTS-UnFun] Call `fn` once per candidate element of a set's element type, passing
    // the element's Z3 key. The domain itself comes from enumerate_set_element_domain
    // (array_domain_utils.hpp), so subset/disjoint/cardinality/set-algebra all agree with
    // enumerate_array_domain on what a set's elements are — they used to hand-roll the
    // bounded-int-vs-objects fork separately, four times over.
    //
    //   // "A subseteq B" over set{integer[0,2]}
    //   for_each_set_element(elem_type, [&](const z3::expr& e) {
    //       conjuncts.push_back(z3::implies(convert_membership(e, A),
    //                                       convert_membership(e, B)));
    //   });                                  // called with e = 0, 1, 2
    void for_each_set_element(const Type* elem_type,
                              const std::function<void(const z3::expr&)>& fn) const;

    // Recursive helper for ExprID-based conversion
    z3::expr convert_node(ExprID id);

    // =====================================================================
    // [XTS] Operator families lifted out of convert_node
    //
    // convert_node grew to ~500 lines from a ~120-line pre-XTS core (leaf nodes,
    // STATE_VARIABLE, and the operator dispatch switch) because each XTS operator
    // family was inserted as an early-return guard ahead of that switch. Those guards
    // are now named, so convert_node's FUNCTION_APPLICATION arm reads as:
    //
    //   if (op == ExprOperator::UNKNOWN) return convert_quantifier(id);
    //   if (auto r = try_convert_set_op(id))       return *r;
    //   if (auto r = try_convert_uf_array_op(id))  return *r;
    //   ...generic argument conversion + dispatch switch...
    //
    // The pre-XTS blocks are deliberately left inline in convert_node: moving them is
    // a separate change with a different risk profile.
    //
    // CONTRACT for the try_* pair: a returned value means "fully handled"; nullopt
    // means "not my shape / not my mode", and the caller falls through to the generic
    // path. Neither ever partially converts.
    // =====================================================================

    // [XTS] forall/exists, which arrive as FUNCTION_APPLICATION with op=UNKNOWN and
    // head symbol "up:forall" / "up:exists". Expanded over the quantified variable's
    // finite range: forall → AND of body instances, exists → OR.
    // Always returns or throws — the caller only reaches it when op == UNKNOWN.
    z3::expr convert_quantifier(ExprID id);

    // [XTS] SET_MEMBER / SET_SUBSETEQ / SET_DISJOINT / SET_CARDINALITY — the set
    // operators that expand over the finite element domain rather than using a
    // quantifier. nullopt when `id` is not one of them.
    std::optional<z3::expr> try_convert_set_op(ExprID id);

    // [XTS-UnFun] UF-mode interceptions that have no Theory-mode counterpart:
    // ARRAY_READ on an array-typed SV root (one UF application instead of a nested
    // select chain), and whole-array/set EQUALS (pointwise, since UF has no
    // extensionality). Returns nullopt in Theory mode and for shapes these don't
    // apply to, so both fall through to the generic switch unchanged.
    std::optional<z3::expr> try_convert_uf_array_op(ExprID id);
    // Helper methods for specific Z3 operations
    z3::expr handle_and(const std::vector<z3::expr>& args);
    z3::expr handle_or(const std::vector<z3::expr>& args);
    z3::expr handle_not(const std::vector<z3::expr>& args);
    z3::expr handle_equals(const std::vector<z3::expr>& args);
    z3::expr handle_less_than(const std::vector<z3::expr>& args);
    z3::expr handle_less_equal(const std::vector<z3::expr>& args);
    z3::expr handle_greater_than(const std::vector<z3::expr>& args);
    z3::expr handle_greater_equal(const std::vector<z3::expr>& args);
    z3::expr handle_plus(const std::vector<z3::expr>& args);
    z3::expr handle_minus(const std::vector<z3::expr>& args);
    z3::expr handle_multiply(const std::vector<z3::expr>& args);
    z3::expr handle_divide(const std::vector<z3::expr>& args);
    z3::expr handle_implies(const std::vector<z3::expr>& args);
    z3::expr handle_count(const std::vector<z3::expr>& args);

    // Recursive membership test: "elem ∈ set_id" → Z3 boolean expression.
    // Handles SV, SET_CONSTANT, SET_UNION, SET_INTERSECT, SET_DIFFERENCE without
    // converting the set expression to an intermediate Z3 array.
    z3::expr convert_membership(const z3::expr& elem, ExprID set_id);

    // Return the element Type* for a set-valued ExprID (SV or binary set op).
    // Used by SET_SUBSETEQ, SET_DISJOINT, SET_CARDINALITY to enumerate the domain.
    const Type* elem_type_of_set_expr(ExprID set_id) const;

    // [XTS-UnFun] Peel a chain of nested ARRAY_READ(ARRAY_READ(...,i)...,j) down to its
    // innermost non-ARRAY_READ base expression, collecting the index ExprIDs outermost
    // first. Mirrors GroundedEncoder::peel_array_write (grounded_encoder.cpp) but for
    // reads instead of writes.
    static ExprID peel_array_read_chain(ExprID id, const ExprPool& pool,
                                         std::vector<ExprID>& indices_out);
};

} // namespace rantanplan
