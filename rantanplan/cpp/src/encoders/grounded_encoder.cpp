#include "grounded_encoder.hpp"
#include "array_domain_utils.hpp" // [XTS-UnFun]
#include "../analysis/interference_analysis.hpp"
#include "../util/stats.hpp"
#include "../config/config.hpp"
#include <algorithm>
#include <iostream>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace rantanplan {

// Constructor
GroundedEncoder::GroundedEncoder(const Problem& problem, z3::context& ctx)
    : problem_(problem), ctx_(ctx), variable_factory_(ctx), grounded_visitor_(ctx_, &problem_, &variable_factory_) {
    layers_encoded_ = -1;
    variable_factory_.set_problem(&problem_);
    build_epc_index();
}

z3::expr GroundedEncoder::convert_expr_id_to_z3(ExprID id, int timestep) {
    return grounded_visitor_.convert_from_pool(id, timestep);
}

// [XTS] Peel ARRAY_WRITE(ARRAY_READ(...ARRAY_READ(SV, i)..., j), k) into its root SV
// and an outermost-first index list [i, j, ..., k].  Works for any nesting depth.
static std::pair<ExprID, std::vector<ExprID>> peel_array_write(ExprID fluent_id, const ExprPool& pool) {
    ExprID base     = pool.argument(fluent_id, 0);
    ExprID last_idx = pool.argument(fluent_id, 1);
    // Traverse the ARRAY_READ chain on base, collecting indices innermost-first.
    std::vector<ExprID> rev;
    ExprID cur = base;
    while (pool.is_function_application(cur) &&
           pool.op(cur) == ExprOperator::ARRAY_READ &&
           pool.argument_count(cur) >= 2) {
        rev.push_back(pool.argument(cur, 1));
        cur = pool.argument(cur, 0);
    }
    // Reverse to outermost-first, then append the ARRAY_WRITE's own index.
    std::vector<ExprID> indices(rev.rbegin(), rev.rend());
    indices.push_back(last_idx);
    return {cur, std::move(indices)};
}

// [XTS] Build a nested z3::store for an N-D cell write.
// store_chain(arr, [i,j,k], val) = store(arr, i, store_chain(select(arr,i), [j,k], val))
z3::expr GroundedEncoder::build_store_chain(const z3::expr& arr,
                                            const std::vector<z3::expr>& indices,
                                            size_t from, const z3::expr& val) {
    if (from == indices.size() - 1)
        return z3::store(arr, indices[from], val);
    return z3::store(arr, indices[from],
                     build_store_chain(z3::select(arr, indices[from]), indices, from + 1, val));
}

// [XTS] Read-side counterpart of build_store_chain: repeatedly select() to fetch an
// N-D cell's current value. Used when an ARRAY_WRITE's value is itself SET_ADD/REMOVE
// (e.g. bins[src] := SetRemove(item, bins[src])) — the target cell holds a *set*, so
// the write needs the cell's current set value first, not just the plain write index.
static z3::expr select_chain(const z3::expr& arr, const std::vector<z3::expr>& indices) {
    z3::expr cell = arr;
    for (const z3::expr& idx : indices) cell = z3::select(cell, idx);
    return cell;
}

// [XTS-UnFun] See header for the contract. Preserves first-occurrence order across
// records so the outer fold in fold_array_writes() below can put the first-seen
// action in write_list as the outermost ITE branch (matches the pre-existing
// Theory-mode Ite ordering exactly).
void GroundedEncoder::group_records_by_action(
        const std::vector<ArrayWriteRecord>& write_list,
        std::vector<const Action*>& action_order,
        std::unordered_map<const Action*, std::vector<const ArrayWriteRecord*>>& by_action) {
    for (const auto& rec : write_list) {
        if (by_action.find(rec.action) == by_action.end())
            action_order.push_back(rec.action);
        by_action[rec.action].push_back(&rec);
    }
}

// [XTS-UnFun] See header for the contract.
z3::expr GroundedEncoder::fold_array_writes(
        const std::vector<ArrayWriteRecord>& write_list, int t, const z3::expr& base_value,
        const std::function<z3::expr(const z3::expr&, const ArrayWriteRecord&)>& resolve_record) {
    std::vector<const Action*> action_order;
    std::unordered_map<const Action*, std::vector<const ArrayWriteRecord*>> by_action;
    group_records_by_action(write_list, action_order, by_action);

    // Outer fold over distinct firing actions, built in reverse order so the first
    // action in write_list ends up as the outermost branch.
    z3::expr update = base_value;
    for (auto it = action_order.rbegin(); it != action_order.rend(); ++it) {
        const Action* action = *it;
        z3::expr act_var = variable_factory_.get_action_variable(*action, t);

        // Inner fold over this action's own records: last-matching-record-wins,
        // via whatever mechanism resolve_record uses (store-chain or index-match ite).
        z3::expr chained = base_value;
        for (const auto* rec : by_action[action]) {
            z3::expr new_val = resolve_record(chained, *rec);
            if (rec->cond_id.valid()) {
                z3::expr cond = convert_expr_id_to_z3(rec->cond_id, t);
                new_val = z3::ite(cond, new_val, chained);
            }
            chained = new_val;
        }
        update = z3::ite(act_var, chained, update);
    }
    return update;
}

// [XTS-UnFun] See header for the contract.
z3::expr GroundedEncoder::build_uf_point_write_fact(ExprID base_sv, const std::vector<ExprID>& indices,
                                                     const z3::expr& val, ExprID cond_id, int t) {
    // Arity is however many indices this write addresses (a whole-set write on a plain
    // set fluent passes {elem}, so arity 1 — matching that fluent's Int->Bool function).
    const Type* vt = problem_.type_for_id(base_sv);
    const z3::func_decl& fn_next =
        grounded_visitor_.uf_for(base_sv, vt, t + 1, static_cast<int>(indices.size()));

    z3::expr_vector idx_z3(ctx_);
    for (ExprID idx : indices) idx_z3.push_back(convert_expr_id_to_z3(idx, t));
    z3::expr fact = (fn_next(idx_z3) == val);
    if (cond_id.valid()) {
        z3::expr cond = convert_expr_id_to_z3(cond_id, t);
        fact = z3::implies(cond, fact);
    }
    return fact;
}

// [XTS-UnFun] See header for the contract.
z3::expr GroundedEncoder::build_uf_whole_assign_fact(ExprID base_sv, const EffectExpression& effect, int t) {
    // Whole-fluent write: arity comes from the type, not from a caller's index list.
    const Type* vt = problem_.type_for_id(base_sv);
    auto domain = enumerate_array_domain(vt, problem_);
    const z3::func_decl& fn_next = grounded_visitor_.uf_for(base_sv, vt, t + 1);

    z3::expr_vector conjuncts(ctx_);
    for (const auto& cell : domain) {
        conjuncts.push_back(fn_next(variable_factory_.cell_args(cell))
                            == grounded_visitor_.convert_array_cell_value(effect.value_id(), cell, t));
    }
    z3::expr fact = conjuncts.empty() ? ctx_.bool_val(true) : z3::mk_and(conjuncts);
    if (effect.is_conditional()) {
        z3::expr cond = convert_expr_id_to_z3(effect.condition_id(), t);
        fact = z3::implies(cond, fact);
    }
    return fact;
}

// Helper function to convert effect to Z3 constraint using visitor
z3::expr GroundedEncoder::convert_effect_to_z3(const EffectExpression& effect, int timestep) {
    const ExprPool& pool = problem_.pool();

    // [XTS] Set write: SET_ADD(elem) → store(set_t, elem, true); SET_REMOVE → false
    if (pool.is_function_application(effect.value_id()) &&
        pool.argument_count(effect.value_id()) >= 1) {
        ExprOperator val_op = pool.op(effect.value_id());
        if (val_op == ExprOperator::SET_ADD || val_op == ExprOperator::SET_REMOVE) {
            z3::expr set_t    = convert_expr_id_to_z3(effect.fluent_id(), timestep);
            z3::expr set_next = convert_expr_id_to_z3(effect.fluent_id(), timestep + 1);
            z3::expr elem     = convert_expr_id_to_z3(pool.argument(effect.value_id(), 0), timestep);
            bool adding       = (val_op == ExprOperator::SET_ADD);
            z3::expr constraint = (set_next == z3::store(set_t, elem, ctx_.bool_val(adding)));
            if (effect.is_conditional())
                constraint = z3::implies(
                    convert_expr_id_to_z3(effect.condition_id(), timestep), constraint);
            return constraint;
        }
    }

    // Scalar path: bool, int, real, object fluents
    z3::expr fluent_curr_z3 = convert_expr_id_to_z3(effect.fluent_id(), timestep);
    z3::expr fluent_next_z3 = convert_expr_id_to_z3(effect.fluent_id(), timestep + 1);
    z3::expr value_z3 = convert_expr_id_to_z3(effect.value_id(), timestep);

    z3::expr effect_constraint = ctx_.bool_val(true);
    switch (effect.kind()) {
        case EffectExpression::Kind::ASSIGN:
            effect_constraint = (fluent_next_z3 == value_z3);
            break;
        case EffectExpression::Kind::INCREASE:
            effect_constraint = (fluent_next_z3 == fluent_curr_z3 + value_z3);
            break;
        case EffectExpression::Kind::DECREASE:
            effect_constraint = (fluent_next_z3 == fluent_curr_z3 - value_z3);
            break;
    }

    if (effect.is_conditional()) {
        z3::expr condition_z3 = convert_expr_id_to_z3(effect.condition_id(), timestep);
        effect_constraint = z3::implies(condition_z3, effect_constraint);
    }

    return effect_constraint;
}

/**
 * @brief Encodes the initial state constraints at timestep 0
 * 
 * Note: The Python side takes responsibility for initializing all fluents 
 * with default values, so here we can just iterate over the initial state.
 */
std::shared_ptr<z3::expr> GroundedEncoder::encode_initial_state() {
   z3::expr_vector initial_state(ctx_);
    auto& stats = Stats::instance();
    const bool uf_mode = variable_factory_.uf_mode();

    // Process each assignment in the initial state at timestep 0
    std::unordered_set<ExprID> init_fluents;
    for (const auto& assignment : problem_.initial_state()) {
        ExprID fluent_id = assignment.fluent_id();
        const Type* fvt = problem_.type_for_id(fluent_id);

        // [XTS-UnFun] Array/set assignment under UF mode: neither side has a single
        // Z3 "value" to compare with a bare ==, so enumerate the domain and equate
        // pointwise instead. convert_array_cell_value handles both operands (the bare
        // fluent SV and whatever literal/expression initializes it) uniformly.
        if (uf_mode && fvt && (fvt->is_array() || fvt->is_set())) {
            auto domain = enumerate_array_domain(fvt, problem_);
            for (const auto& cell : domain) {
                initial_state.push_back(
                    grounded_visitor_.convert_array_cell_value(fluent_id, cell, 0) ==
                    grounded_visitor_.convert_array_cell_value(assignment.value_id(), cell, 0));
            }
        } else {
            z3::expr fluent_expr = convert_expr_id_to_z3(fluent_id, 0);
            z3::expr value_expr = convert_expr_id_to_z3(assignment.value_id(), 0);
            initial_state.push_back(fluent_expr == value_expr);
        }
        init_fluents.insert(fluent_id);
    }

    // [XTS] Default-initialize set/array fluents not mentioned in the initial state.
    // Without this, Z3 treats them as unconstrained, making init non-deterministic.
    std::function<z3::expr(z3::sort, const Type*)> make_default =
        [&](z3::sort s, const Type* t) -> z3::expr {
        if (s.is_bool()) return ctx_.bool_val(false);
        if (s.is_array()) {
            const Type* elem_t = nullptr;
            if (t && t->is_array()) {
                const std::string& ename = t->array_element_type_name();
                elem_t = detail::find_element_type(ename, problem_);
            }
            return z3::const_array(s.array_domain(), make_default(s.array_range(), elem_t));
        }
        if (t) {
            const Type* bi = t->bounded_int_ancestor();
            if (bi) return ctx_.int_val(bi->lower_bound());
        }
        return ctx_.int_val(0);
    };
    for (ExprID eid : problem_.grounded_fluents()) {
        const Type* vt = problem_.type_for_id(eid);
        if (!vt || (!vt->is_set() && !vt->is_array())) continue;
        if (init_fluents.count(eid)) continue;

        // [XTS-UnFun] Pointwise default under UF mode: fn_at_0(cell) == the leaf
        // default value, for every cell in the fluent's static domain. Mirrors
        // make_default's dispatch order (bool -> false; array-of-sets leaf -> empty
        // set; bounded-int -> lower_bound(); else -> 0) but flattened, since
        // enumerate_array_domain already descends to leaf-level index tuples.
        if (uf_mode) {
            auto domain = enumerate_array_domain(vt, problem_);
            if (domain.empty()) continue;
            const z3::func_decl& fn0 = grounded_visitor_.uf_for(eid, vt, 0);

            const Type* leaf = leaf_element_type(vt, problem_);
            z3::expr default_val = ctx_.int_val(0);
            if (vt->is_set() || (leaf && leaf->is_bool())) {
                default_val = ctx_.bool_val(false);
            } else if (leaf && leaf->is_set()) {
                default_val = z3::const_array(ctx_.int_sort(), ctx_.bool_val(false));
            } else if (leaf) {
                const Type* bi = leaf->bounded_int_ancestor();
                if (bi) default_val = ctx_.int_val(bi->lower_bound());
            }

            for (const auto& cell : domain) {
                initial_state.push_back(fn0(variable_factory_.cell_args(cell)) == default_val);
            }
            continue;
        }

        z3::expr var = convert_expr_id_to_z3(eid, 0);
        z3::expr def = make_default(var.get_sort(), vt);
        initial_state.push_back(var == def);
    }

    // Collect statistics
    stats.set("encoder.initial_constraints", initial_state.size());
    
    // Combine all initial state constraints with logical AND
    if (initial_state.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    z3::expr initial_state_formula = z3::mk_and(initial_state);
    return std::make_shared<z3::expr>(initial_state_formula);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_actions(int t) {
    z3::expr_vector action_constraints(ctx_);
    auto& stats = Stats::instance();

    for (const Action& action : problem_.actions()) {
        auto single = encode_single_action(action, t);
        if (single) {
            action_constraints.push_back(*single);
        }
    }

    stats.add("encoder.action_constraints", action_constraints.size());

    if (action_constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    return std::make_shared<z3::expr>(z3::mk_and(action_constraints));
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_single_action(const Action& action, int t) {
    if (action.effects().empty()) return nullptr;

    z3::expr_vector constraints(ctx_);
    z3::expr action_var = variable_factory_.get_action_variable(action, t);

    if (action.has_precondition()) {
        z3::expr z3_precond = convert_expr_id_to_z3(action.precondition_id(), t);
        constraints.push_back(z3::implies(action_var, z3_precond));
    }

    // Delegate to encode_non_cumulative_effects which handles within-action
    // array write chaining (multiple writes to the same array are chained
    // rather than emitted as separate conflicting equalities).
    auto effect_constraint = encode_non_cumulative_effects(action, t);
    if (effect_constraint) {
        constraints.push_back(*effect_constraint);
    }

    if (constraints.empty()) return nullptr;
    return std::make_shared<z3::expr>(z3::mk_and(constraints));
}

// [XTS-UnFun] See header for the contract.
//
// UF + Ite (fold) mode: array/set effects are handled entirely by encode_frames'
// pointwise fold, which asserts both "what changed" and "what stayed the same" in one
// pass per array per timestep. Asserting anything in the effect loop too would be
// redundant at best; for two parallel actions writing distinct cells of the same array
// it would be actively wrong — two independent per-action fn_next(i)==v facts leave
// every *other* cell of fn_next completely unconstrained (nothing outside encode_frames
// ties fn_next to fn_prev), which is silently unsound rather than merely redundant.
//
// UF + Disequality mode does NOT defer: its handlers build a direct point fact per
// effect, and the frame axiom's negative direction supplies "nothing else changed."
bool GroundedEncoder::uf_ite_defers_effect(ExprID fluent_id) const {
    const ExprPool& pool = problem_.pool();
    const bool is_array_write = pool.is_function_application(fluent_id) &&
                                pool.op(fluent_id) == ExprOperator::ARRAY_WRITE &&
                                pool.argument_count(fluent_id) >= 2;
    const bool is_plain_array_fluent  = array_fluent_ids_.count(fluent_id) > 0;
    return is_array_write || is_plain_array_fluent;
}

// [XTS] See header for the contract.
z3::expr GroundedEncoder::array_intermediate_or_current(
        ExprID sv_id, int t, const std::unordered_map<ExprID, z3::expr>& intermediates) {
    auto it = intermediates.find(sv_id);
    if (it != intermediates.end()) return it->second;
    return convert_expr_id_to_z3(sv_id, t);
}

// [XTS] See header for the contract.
bool GroundedEncoder::try_encode_array_write_effect(
        const EffectExpression& effect, int t, bool uf_mode,
        z3::expr_vector& effect_exprs,
        std::unordered_map<ExprID, z3::expr>& intermediates) {
    const ExprPool& pool = problem_.pool();
    ExprID fluent_id = effect.fluent_id();

    // [XTS] N-D array write: peel the ARRAY_READ chain to get root SV + index list.
    if (!(pool.is_function_application(fluent_id) &&
          pool.op(fluent_id) == ExprOperator::ARRAY_WRITE &&
          pool.argument_count(fluent_id) >= 2)) {
        return false;
    }

    auto [base_sv, idx_ids] = peel_array_write(fluent_id, pool);

    // [XTS] Array-of-sets cell mutation: bins[src] := SetRemove(item, bins[src]).
    // SET_ADD/SET_REMOVE aren't standalone-convertible expressions (they only mean
    // something as an effect delta), so — same as the plain-set-fluent fast path in
    // convert_effect_to_z3 — detect them on the value side here instead of falling
    // through to the generic converter.
    ExprID value_id = effect.value_id();
    bool value_is_set_delta = pool.is_function_application(value_id) &&
                              pool.argument_count(value_id) >= 1 &&
                              (pool.op(value_id) == ExprOperator::SET_ADD ||
                               pool.op(value_id) == ExprOperator::SET_REMOVE);

    // [XTS-UnFun] UF + Disequality: one direct point fact, no chaining —
    // see build_uf_point_write_fact's doc comment for why that's sound here.
    if (uf_mode) {
        ExprID cond_id = effect.is_conditional() ? effect.condition_id() : EXPR_NULL;
        if (value_is_set_delta) {
            // UF mode only replaces the *array* dimensions with an uninterpreted
            // function — per resolve_elem_sort_at_depth's documented contract, a cell
            // still holds a native Z3 Set (Array Int Bool), UF does not descend into
            // per-element set membership too. So: read the CURRENT set at this cell
            // (arity = idx_ids.size(), same shape build_uf_point_write_fact would use
            // for a whole-set write), store/remove the element in it Theory-style, and
            // assert that as the point write's new value.
            const Type* vt = problem_.type_for_id(base_sv);
            const z3::func_decl& fn_curr =
                grounded_visitor_.uf_for(base_sv, vt, t, static_cast<int>(idx_ids.size()));
            z3::expr_vector idx_z3(ctx_);
            for (ExprID idx : idx_ids) idx_z3.push_back(convert_expr_id_to_z3(idx, t));
            z3::expr current_set = fn_curr(idx_z3);
            z3::expr elem = convert_expr_id_to_z3(pool.argument(value_id, 0), t);
            bool adding = (pool.op(value_id) == ExprOperator::SET_ADD);
            z3::expr new_set = z3::store(current_set, elem, ctx_.bool_val(adding));
            effect_exprs.push_back(build_uf_point_write_fact(base_sv, idx_ids, new_set, cond_id, t));
        } else {
            z3::expr val = convert_expr_id_to_z3(value_id, t);
            effect_exprs.push_back(build_uf_point_write_fact(base_sv, idx_ids, val, cond_id, t));
        }
        return true;
    }

    // Theory mode: build a nested store chain that updates the intermediate array in
    // place — multiple writes to the same array within one action must be chained
    // (see encode_non_cumulative_effects' array_intermediates).
    z3::expr arr_cur = array_intermediate_or_current(base_sv, t, intermediates);
    std::vector<z3::expr> z3_indices;
    for (ExprID idx : idx_ids) z3_indices.push_back(convert_expr_id_to_z3(idx, t));
    z3::expr val = value_is_set_delta
        ? [&]() -> z3::expr {
              // The cell holds a set: fetch its current value and store/remove the
              // element in it, same as the plain-set-fluent path — the resulting set
              // becomes the value written into the array cell.
              z3::expr current_set = select_chain(arr_cur, z3_indices);
              z3::expr elem = convert_expr_id_to_z3(pool.argument(value_id, 0), t);
              bool adding = (pool.op(value_id) == ExprOperator::SET_ADD);
              return z3::store(current_set, elem, ctx_.bool_val(adding));
          }()
        : convert_expr_id_to_z3(value_id, t);
    z3::expr new_arr = build_store_chain(arr_cur, z3_indices, 0, val);

    if (effect.is_conditional()) {
        z3::expr cond = convert_expr_id_to_z3(effect.condition_id(), t);
        new_arr = z3::ite(cond, new_arr, array_intermediate_or_current(base_sv, t, intermediates));
    }
    intermediates.insert_or_assign(base_sv, new_arr);
    return true;
}

// [XTS-UnFun] See header for the contract.
bool GroundedEncoder::try_encode_uf_whole_fluent_effect(
        const EffectExpression& effect, int t, bool uf_mode,
        z3::expr_vector& effect_exprs) {
    // Theory mode handles both shapes below via convert_effect_to_z3 (unchanged), so
    // this handler is UF-only.
    ExprID fluent_id = effect.fluent_id();
    if (!uf_mode || !array_fluent_ids_.count(fluent_id)) return false;

    const ExprPool& pool = problem_.pool();
    ExprID value_id = effect.value_id();
    if (pool.is_function_application(value_id) && pool.argument_count(value_id) >= 1) {
        ExprOperator val_op = pool.op(value_id);
        if (val_op == ExprOperator::SET_ADD || val_op == ExprOperator::SET_REMOVE) {
            ExprID elem_id = pool.argument(value_id, 0);
            bool adding = (val_op == ExprOperator::SET_ADD);
            ExprID cond_id = effect.is_conditional() ? effect.condition_id() : EXPR_NULL;
            effect_exprs.push_back(build_uf_point_write_fact(
                fluent_id, {elem_id}, ctx_.bool_val(adding), cond_id, t));
            return true;
        }
    }
    effect_exprs.push_back(build_uf_whole_assign_fact(fluent_id, effect, t));
    return true;
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_non_cumulative_effects(const Action& action, int t) {
    if (action.effects().empty()) return nullptr;

    z3::expr action_var = variable_factory_.get_action_variable(action, t);

    // Multiple effects on the same array (e.g. forall-expanded writes to cells[0..3])
    // must be chained: each write builds on the result of the previous one.
    // Emitting separate arr_{t+1}==store(arr_t,i,v) constraints per effect
    // produces conflicting equalities that Z3 can only jointly satisfy when
    // all written indices happen to be equal — making the action unfireable.
    // Only the Theory-mode handlers populate this; under UF it stays empty, which is
    // what makes the flush loop at the bottom a no-op there.
    std::unordered_map<ExprID, z3::expr> array_intermediates;

    z3::expr_vector effect_exprs(ctx_);
    for (const Effect& eff_wrapper : action.effects()) {
        const EffectExpression& effect = eff_wrapper.effect_expression();
        const bool uf_mode = variable_factory_.uf_mode();

        // [XTS-UnFun] UF+Ite hands every array/set effect to encode_frames' pointwise
        // fold instead — see uf_ite_defers_effect for why emitting here is unsound.
        if (uf_mode && array_frame_mode_ == ArrayFrameMode::Ite &&
            uf_ite_defers_effect(effect.fluent_id())) {
            continue;
        }

        // [XTS] Array/set effect shapes, most specific first. Each handler returns true
        // once it has fully handled the effect; see the contract block in the header.
        if (try_encode_array_write_effect(effect, t, uf_mode, effect_exprs, array_intermediates))
            continue;
        if (try_encode_uf_whole_fluent_effect(effect, t, uf_mode, effect_exprs))
            continue;

        // Scalar (pre-XTS) path: bool, int, real and object fluents.
        effect_exprs.push_back(convert_effect_to_z3(effect, t));
    }

    // Flush chained array writes: arr_{t+1} == final_intermediate
    for (auto& [sv_id, final_arr] : array_intermediates) {
        z3::expr arr_next = convert_expr_id_to_z3(sv_id, t + 1);
        effect_exprs.push_back(arr_next == final_arr);
    }

    if (effect_exprs.empty()) return nullptr;
    return std::make_shared<z3::expr>(z3::implies(action_var, z3::mk_and(effect_exprs)));
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_cumulative_effects(int t) {
    return std::make_shared<z3::expr>(ctx_.bool_val(true));
}

void GroundedEncoder::ensure_action_variables(int t) {
    for (const Action& action : problem_.actions()) {
        variable_factory_.get_action_variable(action, t);
    }
}

/**
 * @brief Encodes frame axioms for a specific time step
 *
 * Frame axioms ensure that fluents only change when explicitly caused by
 * actions.  For each fluent in the EPC index, adds a constraint that says: "if a
 * fluent changes between time t and t+1, then at least one action that can
 * cause this change must be executed at time t".
 *
 * (at_robot_A^t != at_robot_A^(t+1)) -> (move_A_to_B^t || (Effect_precondition^t && conditional_action^t))
 *
 * @param t The current time step for which to encode frame axioms
 * @return A shared pointer to a Z3 expression representing the conjunction of all frame
 * axioms for the transition from time t to t+1. Returns true if no fluents
 * exist in the EPC index.
 */
std::shared_ptr<z3::expr> GroundedEncoder::encode_frames(int t) {
    // Fast path (pre-XTS behavior): with the lazy FrameAxiomModule active and no
    // array/set fluents in the problem, everything below is the module's job —
    // scalars are skipped explicitly and the array loops iterate empty containers.
    // array_fluent_ids_ is populated once by build_epc_index, so this costs one
    // set lookup per call.
    if (lazy_frames_enabled_ && array_fluent_ids_.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }

    std::vector<z3::expr> frame_axioms;
    auto& stats = Stats::instance();

    // [XTS] One frame-axiom shape for every fluent kind, scalar or array/set:
    //   (f^t != f^{t+1}) -> (a_1 || (a_2 && c_2) || ... || a_n)
    // "!=" works for any Z3 sort (Bool/Int/Array), so array/set fluents need no
    // special-cased ITE-of-stores here. Their (action, effect) pairs are
    // registered in epc_index_ exactly like scalars, keyed by the root array/set
    // SV (see build_epc_index). The actual new value on a write still comes from
    // the per-action effect axiom in encode_non_cumulative_effects
    // (A -> arr_{t+1} = store(arr_t, i, v)); this lambda only builds the other
    // half of the frame axiom: "if it changed, some action explains it".
    auto build_frame_axiom = [&](ExprID eid) {
        auto epc_it = epc_index_.find(eid);
        if (epc_it == epc_index_.end()) return;
        const auto& action_effects = epc_it->second;

        // Fluent variables at timesteps t and t+1.
        z3::expr fluent_t = convert_expr_id_to_z3(eid, t);
        z3::expr fluent_t_plus_1 = convert_expr_id_to_z3(eid, t + 1);

        // For boolean fluents, an alternative "direction-aware" encoding splits
        // this into two clauses by effect polarity — one with only the actions
        // that set f to true, one with only those that set f to false.  These
        // are derivable by resolving the combined frame clause with the effect
        // axioms and are strictly shorter.  However, a full experiment on 417
        // instances (branch frame-polarity, Apr 2026) showed this is 16% slower
        // on average (geo mean 1.16, 89 losses vs 45 wins).  The shorter
        // clauses disrupt Z3's VSIDS heuristics more than they help BCP,
        // especially on boolean-heavy domains (42% slower, 3:1 loss ratio).
        // Mixed boolean/numeric domains benefit slightly (geo 0.93) but not
        // enough to offset the overall loss.  Conclusion: keep the combined
        // encoding.
        z3::expr fluent_changed = (fluent_t != fluent_t_plus_1);

        // Build disjunction of all actions that can cause this change.
        z3::expr_vector action_vector(ctx_);
        for (const auto& [action, effect_expr] : action_effects) {
            z3::expr action_var = variable_factory_.get_action_variable(*action, t);
            if (effect_expr->is_conditional()) {
                // Conditional effect: only explains the change when its condition also holds.
                z3::expr condition_z3 = convert_expr_id_to_z3(effect_expr->condition_id(), t);
                action_vector.push_back(action_var && condition_z3);
            } else {
                action_vector.push_back(action_var);
            }
        }
        z3::expr action_disjunction = action_vector.empty()
            ? ctx_.bool_val(false)
            : z3::mk_or(action_vector);

        // (fluent^t != fluent^(t+1)) -> (a1 || (c2 && a2) || a3 || ...)
        frame_axioms.push_back(z3::implies(fluent_changed, action_disjunction));
    };

    // Scalar fluents: emitted eagerly only when FrameAxiomModule is NOT active.
    // When it is active (lazy_frames_enabled_), the module watches these lazily
    // and handles conflicts / preservation via the propagator.
    if (!lazy_frames_enabled_) {
        for (ExprID eid : problem_.grounded_fluents()) {
            // [XTS] Array/set fluents get their frame axiom in the loop below,
            // unconditionally (not gated on lazy_frames_enabled_) — skip them here.
            if (array_fluent_ids_.count(eid)) continue;

            build_frame_axiom(eid);
        }
    } // end scalar fluent loop (eager mode only)

    // [XTS-UnFun] Array/set fluents under --array-encoding uf: array_frame_mode_
    // selects between the same two shapes Theory mode offers, both applied
    // pointwise (per enumerated cell) instead of to a whole array term, since UF
    // function declarations have no first-class equality/disequality the way Array
    // theory's Array-sorted values do (there is no such Z3 term as "fn_t != fn_next").
    // Per-array setup (domain, arity, the two func_decls) is identical for both
    // sub-modes, so it's resolved once per array here; only the per-cell body below
    // differs between them.
    if (variable_factory_.uf_mode()) {
        for (const auto& [base_sv_id, write_list] : array_epc_index_) {
            const Type* vt = problem_.type_for_id(base_sv_id);
            if (!vt) continue; // defensive; every array_epc_index_ key should have a type

            auto domain = enumerate_array_domain(vt, problem_);
            if (domain.empty()) continue; // degenerate/empty domain: nothing to constrain

            // arity is needed on its own below (cell_match bounds-checks against it), so
            // it comes from resolve_uf_shape here rather than being left inside uf_for.
            const unsigned arity = variable_factory_.resolve_uf_shape(vt).first;

            Fluent fl = grounded_visitor_.build_grounded_fluent(base_sv_id);
            const z3::func_decl& fn_prev = grounded_visitor_.uf_for(fl, vt, t);
            const z3::func_decl& fn_next = grounded_visitor_.uf_for(fl, vt, t + 1);

            // [XTS-UnFun] cell_match: "does this record write the cell we are
            // constraining?" — a plain zip of the record's cell coordinates against this
            // cell's index tuple. Both are in the same convention now that
            // ArrayWriteRecord keeps an array-of-sets element in set_elem_id instead of
            // appending it to `indices` (see the header), so rec.indices.size() is always
            // either `arity` (a point write) or 0 (a whole-array/set ASSIGN, handled by
            // the callers below). The std::min is therefore unreachable defence, kept
            // only because overrunning cell_args would raise Z3_IOB, which aborts the
            // process instead of surfacing as an error.
            auto cell_match = [&](const ArrayWriteRecord& rec,
                                  const z3::expr_vector& cell_args) -> z3::expr {
                size_t dims = std::min(rec.indices.size(), static_cast<size_t>(arity));
                z3::expr match = ctx_.bool_val(true);
                for (size_t d = 0; d < dims; ++d) {
                    z3::expr idx_z3 = convert_expr_id_to_z3(rec.indices[d], t);
                    match = match && (idx_z3 == cell_args[static_cast<unsigned>(d)]);
                }
                return match;
            };

            if (array_frame_mode_ == ArrayFrameMode::Disequality) {
                // [XTS-UnFun] Pointwise analogue of build_frame_axiom's scalar shape:
                //   (fn_t(cell) != fn_next(cell)) -> (a1 || (a2 && c2) || ...)
                for (const auto& cell : domain) {
                    z3::expr_vector cell_args = variable_factory_.cell_args(cell);

                    z3::expr_vector action_vector(ctx_);
                    for (const auto& rec : write_list) {
                        z3::expr term = variable_factory_.get_action_variable(*rec.action, t);
                        if (rec.cond_id.valid())
                            term = term && convert_expr_id_to_z3(rec.cond_id, t);
                        if (!rec.indices.empty()) {
                            term = term && cell_match(rec, cell_args);
                        }
                        // rec.indices.empty() (whole-array/set ASSIGN) touches every cell unconditionally
                        action_vector.push_back(term);
                    }
                    z3::expr disjunction = action_vector.empty()
                        ? ctx_.bool_val(false)
                        : z3::mk_or(action_vector);
                    frame_axioms.push_back(
                        z3::implies(fn_prev(cell_args) != fn_next(cell_args), disjunction));
                }
            } else {
                // [XTS-UnFun] Ite (fold) mode for UF: asserts, per cell,
                //   fn_next(cell) == <fold-chain over firing actions' writes to that
                //                      cell, or fn_prev(cell) if none of them touch it>
                for (const auto& cell : domain) {
                    z3::expr_vector cell_args = variable_factory_.cell_args(cell);
                    z3::expr prev_val = fn_prev(cell_args);
                    z3::expr update = fold_array_writes(write_list, t, prev_val,
                        [&](const z3::expr& chained, const ArrayWriteRecord& rec) -> z3::expr {
                            if (rec.indices.empty()) {
                                return grounded_visitor_.convert_array_cell_value(rec.val_id, cell, t);
                            }
                            z3::expr match = cell_match(rec, cell_args);
                            // [XTS-UnFun] Read straight off the record now — no
                            // arity arithmetic needed to recover the element.
                            ExprID elem_id = rec.set_elem_id;
                            z3::expr val = ctx_.bool_val(rec.set_value);
                            if (rec.val_id.valid()) {
                                val = convert_expr_id_to_z3(rec.val_id, t);
                            } else if (elem_id.valid()) {
                                // Array-of-sets cell mutation: this cell holds a native
                                // Array(Int,Bool) set, so the delta is a store INTO the
                                // cell's current value (`chained`), not a bare Bool.
                                // The bare-Bool form above is only correct for a plain
                                // set fluent, whose UF function has range Bool.
                                val = z3::store(chained, convert_expr_id_to_z3(elem_id, t),
                                                ctx_.bool_val(rec.set_value));
                            }
                            return z3::ite(match, val, chained);
                        });
                    frame_axioms.push_back(fn_next(cell_args) == update);
                }
            }
        }
    } else if (array_frame_mode_ == ArrayFrameMode::Disequality) {
        // [XTS] Default: same shape as scalars
        for (ExprID eid : array_fluent_ids_) {
            build_frame_axiom(eid);
        }
    } else {
        // For each array/set SV with write records (a1,idx1,val1), (a2,idx2,val2), ...:
        //   arr_{t+1} = ite(guard1, store(arr_t, idx1, val1),
        //               ite(guard2, store(arr_t, idx2, val2),
        //                 ... arr_t))
        for (const auto& [base_sv_id, write_list] : array_epc_index_) {
            z3::expr arr_t    = convert_expr_id_to_z3(base_sv_id, t);
            z3::expr arr_next = convert_expr_id_to_z3(base_sv_id, t + 1);
            z3::expr update = fold_array_writes(write_list, t, arr_t,
                [&](const z3::expr& chained, const ArrayWriteRecord& rec) -> z3::expr {
                    if (rec.indices.empty())
                        return convert_expr_id_to_z3(rec.val_id, t); // whole-array ASSIGN
                    // [XTS-UnFun] store_path(), not indices: for an array-of-sets cell
                    // mutation Theory wants to store all the way down to the set element
                    // (store(arr, i, store(select(arr,i), elem, adding))), which is what
                    // the old combined index list encoded. See ArrayWriteRecord.
                    std::vector<z3::expr> z3_idx;
                    for (ExprID idx : rec.store_path()) z3_idx.push_back(convert_expr_id_to_z3(idx, t));
                    z3::expr val = rec.val_id.valid()
                        ? convert_expr_id_to_z3(rec.val_id, t)
                        : ctx_.bool_val(rec.set_value);
                    return build_store_chain(chained, z3_idx, 0, val);
                });
            frame_axioms.push_back(arr_next == update);
        }
    }

    // Collect statistics
    stats.add("encoder.frame_axioms", frame_axioms.size());
    
    // Combine all frame axioms with logical AND
    if (frame_axioms.empty()) {
        auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
        return expr;
    }
    
    // Create a flat conjunction using Z3's mk_and function
    z3::expr_vector frame_vector(ctx_);
    for (const auto& axiom : frame_axioms) {
        frame_vector.push_back(axiom);
    }
    z3::expr big_and = z3::mk_and(frame_vector);
    return std::make_shared<z3::expr>(big_and);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_goal(int t) {
    // Retrieve goals from the problem
    const auto& goals = problem_.goals();
    auto& stats = Stats::instance();
    
    if (goals.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true)); // If no goals, vacuously satisfied
    }
    
    // Convert each goal expression to Z3 formula and collect them
    std::vector<z3::expr> goal_formulas;
    goal_formulas.reserve(goals.size());
    
    for (const auto& goal : goals) {
        goal_formulas.push_back(convert_expr_id_to_z3(goal.goal_id(), t));
    }
    
    // Collect statistics
    stats.set("encoder.goal_constraints", goal_formulas.size());
    
    // Combine all goal formulas with logical AND
    if (goal_formulas.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    
    // Create a flat conjunction using Z3's mk_and function
    z3::expr_vector goal_vector(ctx_);
    for (const auto& goal : goal_formulas) {
        goal_vector.push_back(goal);
    }
    z3::expr goal_conjunction = z3::mk_and(goal_vector);
    return std::make_shared<z3::expr>(goal_conjunction);
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_parallelism(int t) {
    return parallelism_strategy_->encode_parallelism(t);
}
std::shared_ptr<z3::expr> GroundedEncoder::encode_prefix_monotone(int t) {
    // Build: (∀a. ¬a@t)  →  (∀a. ¬a@(t+1))
    // i.e., if no action fires at t, then no action may fire at t+1.
    // Chained across all t during search, this front-loads all active steps
    // to the prefix of the plan (0..k-1), with empty steps (k..h-1) at the end.
    // Frame axioms then propagate the goal state through the empty suffix,
    // so a single goal literal placed at horizon h witnesses any plan of
    // length k <= h — enabling the horizon schedule's single-literal batching.
    z3::expr_vector no_action_t(ctx_), no_action_t1(ctx_);
    for (const Action& a : problem_.actions()) {
        no_action_t.push_back(!variable_factory_.get_action_variable(a, t));
        no_action_t1.push_back(!variable_factory_.get_action_variable(a, t + 1));
    }
    if (no_action_t.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    return std::make_shared<z3::expr>(
        z3::implies(z3::mk_and(no_action_t), z3::mk_and(no_action_t1))
    );
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_symmetries(int t) {
    auto& stats = Stats::instance();

    if (symmetry_data_.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }

    z3::expr_vector symmetry_constraints(ctx_);
    int ordering_constraints_count = 0;

    for (const auto& sym : symmetry_data_) {
        // LHS: all variable pairs have the same value (symmetric state)
        z3::expr_vector variable_equality_constraints(ctx_);
        for (const auto& [var1_eid, var2_eid] : sym.variable_pairs) {
            z3::expr var1_z3 = convert_expr_id_to_z3(var1_eid, t);
            z3::expr var2_z3 = convert_expr_id_to_z3(var2_eid, t);
            variable_equality_constraints.push_back(var1_z3 == var2_z3);
        }

        // RHS: lexicographic ordering on action pairs
        z3::expr_vector action_ordering_constraints(ctx_);
        for (const auto& action_pair : sym.action_pairs) {
            z3::expr action1_var = variable_factory_.get_action_variable(*action_pair.action1, t);
            z3::expr action2_var = variable_factory_.get_action_variable(*action_pair.action2, t);

            std::string name1 = variable_factory_.get_action_var_name(*action_pair.action1);
            std::string name2 = variable_factory_.get_action_var_name(*action_pair.action2);
            z3::expr ordering_constraint = (name1 < name2) ?
                z3::implies(action1_var, action2_var) :
                z3::implies(action2_var, action1_var);
            action_ordering_constraints.push_back(ordering_constraint);
            ordering_constraints_count++;
        }

        // (all variables are symmetric) => (lexicographic ordering on actions)
        if (!variable_equality_constraints.empty() && !action_ordering_constraints.empty()) {
            z3::expr lhs = z3::mk_and(variable_equality_constraints);
            z3::expr rhs = z3::mk_and(action_ordering_constraints);
            symmetry_constraints.push_back(z3::implies(lhs, rhs));
        }
    }

    stats.add("encoder.symmetry_ordering_constraints", ordering_constraints_count);

    if (symmetry_constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    return std::make_shared<z3::expr>(z3::mk_and(symmetry_constraints));
}

// Build the cache of (ExprID, lo, hi) for every ground fluent whose declared
// value type (or a type-hierarchy ancestor) is a bounded integer.
// Also builds array_elem_bounds_cache_ for 1-D array fluents whose element
// type is a bounded integer — these need per-cell lo/hi constraints.
// Populated once by scanning the initial state, which covers all ground fluent
// instances because _initialize_fluents initialises every fluent to a default.
void GroundedEncoder::build_type_bounds_cache() const {
    if (type_bounds_built_) return;
    type_bounds_built_ = true;

    const auto& pool = problem_.pool();

    for (const auto& assignment : problem_.initial_state()) {
        const ExprID fid = assignment.fluent_id();
        if (!pool.is_state_variable(fid)) continue;

        const ExprID head = pool.head_symbol_id(fid);
        if (!pool.is_fluent_symbol(head)) continue;

        const Fluent* schema = problem_.find_fluent(pool.payload_string(head));
        if (!schema) continue;

        const Type* vt = schema->value_type();
        if (!vt) continue;

        // Scalar bounded-int fluent.
        const Type* bounded = vt->bounded_int_ancestor();
        if (bounded) {
            type_bounds_cache_.push_back({fid, bounded->lower_bound(), bounded->upper_bound()});
            continue;
        }

        // 1-D array fluent with bounded-int element type.
        if (vt->is_array()) {
            int64_t sz = vt->array_size();
            if (sz <= 0) continue;
            const std::string& ename = vt->array_element_type_name();
            const Type* elem = detail::find_element_type(ename, problem_);
            if (!elem) continue;
            const Type* elem_bi = elem->bounded_int_ancestor();
            if (!elem_bi) continue;
            array_elem_bounds_cache_.push_back(
                {fid, sz, elem_bi->lower_bound(), elem_bi->upper_bound()});
        }
    }
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_state_constraints(int t) {
    build_type_bounds_cache();

    const bool has_structural = !state_constraints_.empty();
    // Both caches feed append_type_bound_constraints below, and encode_type_bounds
    // (goal layer h) gates on both -- omitting array_elem_bounds_cache_ here would
    // take the early return and leave per-cell bounds unconstrained at every t < h.
    const bool has_type_bounds =
        !type_bounds_cache_.empty() || !array_elem_bounds_cache_.empty();

    if (!has_structural && !has_type_bounds) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }

    z3::expr_vector conjuncts(ctx_);

    // Mutex constraints: atmost(members, 1) and optionally atleast(members, 1)
    for (const auto& mutex : state_constraints_.mutexes) {
        z3::expr_vector group(ctx_);
        for (ExprID member : mutex.members) {
            group.push_back(convert_expr_id_to_z3(member, t));
        }
        conjuncts.push_back(z3::atmost(group, 1));
        if (mutex.exactly_one) {
            conjuncts.push_back(z3::atleast(group, 1));
        }
    }

    // Numeric bound constraints
    for (const auto& bound : state_constraints_.bounds) {
        z3::expr fluent = convert_expr_id_to_z3(bound.fluent_id, t);
        z3::expr val = fluent.is_int()
            ? ctx_.int_val(static_cast<int64_t>(bound.bound))
            : ctx_.real_val(std::to_string(bound.bound).c_str());
        if (bound.is_lower) {
            conjuncts.push_back(fluent >= val);
        } else {
            conjuncts.push_back(fluent <= val);
        }
    }

    // Object fluent domain constraints: restrict to reachable values
    for (const auto& dc : state_constraints_.domains) {
        z3::expr fluent = convert_expr_id_to_z3(dc.fluent_id, t);
        z3::expr_vector valid(ctx_);
        for (int idx : dc.valid_object_indices) {
            z3::expr val = fluent.is_int()
                ? ctx_.int_val(idx)
                : ctx_.real_val(idx);
            valid.push_back(fluent == val);
        }
        if (!valid.empty()) {
            conjuncts.push_back(z3::mk_or(valid));
        }
    }

    // Conservation law constraints: f_t + g_t == C
    for (const auto& cons : state_constraints_.conservations) {
        z3::expr f = convert_expr_id_to_z3(cons.fluent1_id, t);
        z3::expr g = convert_expr_id_to_z3(cons.fluent2_id, t);
        z3::expr c = f.is_int()
            ? ctx_.int_val(static_cast<int64_t>(cons.constant))
            : ctx_.real_val(std::to_string(cons.constant).c_str());
        conjuncts.push_back((f + g) == c);
    }

    // Declared type range constraints (bounded-int scalars and array cells).
    append_type_bound_constraints(t, conjuncts);

    if (conjuncts.empty()) {
        return std::make_shared<z3::expr>(ctx_.bool_val(true));
    }
    return std::make_shared<z3::expr>(z3::mk_and(conjuncts));
}

// [XTS] Declared type range constraints, shared between encode_state_constraints
// (every t < h, via add_timestep_constraints) and encode_type_bounds (goal state h).
void GroundedEncoder::append_type_bound_constraints(int t, z3::expr_vector& conjuncts) {
    // Scalar bounded-int fluents: lo <= fluent_t <= hi.
    for (const auto& tb : type_bounds_cache_) {
        z3::expr var = convert_expr_id_to_z3(tb.fluent_id, t);
        if (var.is_int()) {
            conjuncts.push_back(var >= ctx_.int_val(tb.lo));
            conjuncts.push_back(var <= ctx_.int_val(tb.hi));
        } else {
            conjuncts.push_back(var >= ctx_.real_val(static_cast<int>(tb.lo)));
            conjuncts.push_back(var <= ctx_.real_val(static_cast<int>(tb.hi)));
        }
    }

    // Per-cell bounds for 1-D array fluents with bounded-int element types.
    // Z3 arrays are total functions (Theory mode) / UF functions are unconstrained
    // outside asserted facts (UF mode), so out-of-range writes are otherwise silent
    // in either encoding.
    for (const auto& ab : array_elem_bounds_cache_) {
        // [XTS-UnFun] array_elem_bounds_cache_ only ever holds 1-D arrays (see
        // build_type_bounds_cache), so arity is always 1 here.
        if (variable_factory_.uf_mode()) {
            const Type* vt = problem_.type_for_id(ab.fluent_id);
            // Only 1-D bounded-int arrays reach here (see build_type_bounds_cache), so
            // the arity is a literal 1 rather than derived from the type.
            const z3::func_decl& fn = grounded_visitor_.uf_for(ab.fluent_id, vt, t, 1);
            for (int64_t i = 0; i < ab.size; ++i) {
                z3::expr cell = fn(ctx_.int_val(i));
                conjuncts.push_back(cell >= ctx_.int_val(ab.lo));
                conjuncts.push_back(cell <= ctx_.int_val(ab.hi));
            }
            continue;
        }
        z3::expr arr = convert_expr_id_to_z3(ab.fluent_id, t);
        for (int64_t i = 0; i < ab.size; ++i) {
            z3::expr cell = z3::select(arr, ctx_.int_val(i));
            conjuncts.push_back(cell >= ctx_.int_val(ab.lo));
            conjuncts.push_back(cell <= ctx_.int_val(ab.hi));
        }
    }
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_type_bounds(int t) {
    build_type_bounds_cache();
    if (type_bounds_cache_.empty() && array_elem_bounds_cache_.empty()) {
        return nullptr;  // no bounded types — goal state stays exactly as pre-XTS
    }
    z3::expr_vector conjuncts(ctx_);
    append_type_bound_constraints(t, conjuncts);
    if (conjuncts.empty()) return nullptr;
    return std::make_shared<z3::expr>(z3::mk_and(conjuncts));
}

void GroundedEncoder::set_parallelism_strategy(std::unique_ptr<ParallelismStrategy> strategy) {
    parallelism_strategy_ = std::move(strategy);
    // Initialize the strategy with problem context
    parallelism_strategy_->initialize(problem_, ctx_, variable_factory_);
}

std::string GroundedEncoder::get_parallelism_strategy_name() const {
    if (parallelism_strategy_) {
        return parallelism_strategy_->get_name();
    }
    return "Unknown";
}

// [XTS] See header. Stage 1 of build_epc_index.
//
// Seeds both indices and collects array/set + IPAR-cell fluent ids in one pass:
// - epc_index_: every fluent gets an empty entry → (f_t != f_{t+1}) → false for statics.
// - array_fluent_ids_: [XTS] marks array/set-typed fluents so encode_frames always
//   builds their frame axiom (never delegated to the lazy FrameAxiomModule, unlike
//   scalars).
// - array_epc_index_: [XTS] array/set fluents get an empty write list →
//   arr_{t+1}==arr_t for statics, used only when array_frame_mode_ == Ite.
void GroundedEncoder::seed_fluent_indices() {
    for (ExprID eid : problem_.grounded_fluents()) {
        epc_index_[eid] = std::vector<std::pair<const Action*, const EffectExpression*>>();
        const Type* vt = problem_.type_for_id(eid);

        if (vt && (vt->is_array() || vt->is_set())) {
            array_fluent_ids_.insert(eid);
            array_epc_index_.emplace(eid, std::vector<ArrayWriteRecord>{});
        }
    }
}

// [XTS] See header.
void GroundedEncoder::index_fluent(ExprID key, const Action* action,
                                    const EffectExpression* eff_expr) {
    epc_index_[key].emplace_back(action, eff_expr);
}

// [XTS] See header.
void GroundedEncoder::record_array_write(const Action* action, ExprID cond, ExprID sv,
                                          std::vector<ExprID> idxs, ExprID val, bool adding,
                                          ExprID set_elem) {
    array_epc_index_[sv].push_back({action, cond, sv, std::move(idxs), val, adding, set_elem});
}

// [XTS] See header. Stage 2 of build_epc_index: the four array/set write shapes.
bool GroundedEncoder::try_record_array_effect(const Action& action,
                                               const EffectExpression& eff_expr) {
    const ExprPool& pool = problem_.pool();
    ExprID fluent_id = eff_expr.fluent_id();
    ExprID value_id  = eff_expr.value_id();
    ExprID cond_id   = eff_expr.is_conditional() ? eff_expr.condition_id() : EXPR_NULL;

    // [XTS] N-D array cell write: ARRAY_WRITE(ARRAY_READ(board,1),2) := val
    //   peel → root=board. The frame axiom belongs to "board", not to this particular
    //   write expression, so index under the peeled root SV.
    if (pool.is_function_application(fluent_id) &&
        pool.op(fluent_id) == ExprOperator::ARRAY_WRITE &&
        pool.argument_count(fluent_id) >= 2) {
        auto [base_sv_id, indices] = peel_array_write(fluent_id, pool);
        array_fluent_ids_.insert(base_sv_id); // in case type_for_id(base_sv_id) missed it
        index_fluent(base_sv_id, &action, &eff_expr);

        // [XTS] Array-of-sets cell mutation: bins[src] := SetRemove(item, bins[src]).
        // value_id is SET_ADD/SET_REMOVE, not a standalone-convertible expression (same
        // reasoning as the plain-set-fluent case below), so the element is recorded on
        // its own field rather than as a value.
        //
        // [XTS-UnFun] The element goes in set_elem_id, NOT appended to `indices`:
        // `indices` stays the array cell coordinates ({src}), which is what UF's
        // per-cell frame axiom needs. Theory recovers the old combined path via
        // rec.store_path(). See ArrayWriteRecord in the header for why the two readings
        // had to be split apart.
        if (pool.is_function_application(value_id) && pool.argument_count(value_id) >= 1) {
            ExprOperator val_op = pool.op(value_id);
            if (val_op == ExprOperator::SET_ADD || val_op == ExprOperator::SET_REMOVE) {
                ExprID elem_id = pool.argument(value_id, 0);
                bool adding    = (val_op == ExprOperator::SET_ADD);
                record_array_write(&action, cond_id, base_sv_id, indices, EXPR_NULL, adding,
                                   /*set_elem=*/elem_id);
                return true;
            }
        }

        record_array_write(&action, cond_id, base_sv_id, indices, value_id, false);
        return true;
    }

    // [XTS] Set point-write: (add elem bag) → SET_ADD(elem). fluent_id is already the
    // set SV itself here (the write is wrapped in value_id, not fluent_id), so no
    // peeling needed. Falls through when the value isn't a set delta.
    if (pool.is_function_application(value_id) && pool.argument_count(value_id) >= 1) {
        ExprOperator val_op = pool.op(value_id);
        if (val_op == ExprOperator::SET_ADD || val_op == ExprOperator::SET_REMOVE) {
            ExprID elem_id = pool.argument(value_id, 0);
            bool adding    = (val_op == ExprOperator::SET_ADD);
            index_fluent(fluent_id, &action, &eff_expr);
            record_array_write(&action, cond_id, fluent_id, {elem_id}, EXPR_NULL, adding);
            return true;
        }
    }

    // [XTS] Whole-array/set ASSIGN: board := ARRAY_CONSTANT(0,0,0).
    // fluent_id is already the array/set SV itself.
    if (pool.is_state_variable(fluent_id)) {
        const Type* vt = problem_.type_for_id(fluent_id);
        if (vt && (vt->is_set() || vt->is_array())) {
            index_fluent(fluent_id, &action, &eff_expr);
            record_array_write(&action, cond_id, fluent_id, {}, value_id, false);
            return true;
        }
    }

    return false; // plain scalar effect — caller indexes it directly
}

// [XTS] See header. Stage 3 of build_epc_index.
//
// Modeling-error check: two UNCONDITIONAL writes from the same action to the same
// literal cell (or two whole-array/set assigns) can never both hold — under
// UF-Disequality they make the action silently unfireable (fn'(i)=v ∧ fn'(i)=w), and
// under Theory they conflict through the store equations (PDDL-XTS test
// X_double_write_same_cell). Conditional writes are skipped: their guards can
// legitimately be disjoint. Warning only — the encoding stays sound either way (the
// action just can't fire).
void GroundedEncoder::warn_duplicate_cell_writes() const {
    const ExprPool& pool = problem_.pool();

    for (const auto& [sv_id, records] : array_epc_index_) {
        std::unordered_map<std::string, int> unconditional_writes_per_cell;
        for (const auto& rec : records) {
            if (rec.cond_id.valid()) continue;  // conditional — legitimate
            std::string key = rec.action->name();
            bool all_const = true;
            // [XTS-UnFun] store_path(), not indices: the set element is part of a cell's
            // identity for this check. Adding two DIFFERENT elements to the same
            // array-of-sets cell is legitimate, and keying on indices alone would report
            // it as a conflicting double write.
            for (ExprID ix : rec.store_path()) {
                if (pool.is_constant(ix) && pool.payload_is_int(ix)) {
                    key += "[" + std::to_string(pool.payload_int(ix)) + "]";
                } else {
                    all_const = false;
                    break;
                }
            }
            if (!all_const) continue;  // dynamic index — can't compare statically
            if (++unconditional_writes_per_cell[key] == 2) {
                std::cerr << "Warning: action '" << rec.action->name()
                          << "' writes the same cell of '"
                          << problem_.pool().to_string(sv_id)
                          << "' unconditionally more than once — the action can "
                             "never fire (conflicting effect values)." << std::endl;
            }
        }
    }
}

void GroundedEncoder::build_epc_index() {
    epc_index_.clear();
    array_fluent_ids_.clear();
    array_epc_index_.clear();

    seed_fluent_indices();

    for (const auto& action : problem_.actions()) {
        for (const auto& effect : action.effects()) {
            const EffectExpression& eff_expr = effect.effect_expression();

            // [XTS] Array/set/IPAR write shapes — see try_record_array_effect.
            if (try_record_array_effect(action, eff_expr)) continue;

            // Scalar: at_robot_A := true → epc_index_[at_robot_A] += (move_A, eff)
            index_fluent(eff_expr.fluent_id(), &action, &eff_expr);
        }
    }

    warn_duplicate_cell_writes();
}

Plan GroundedEncoder::extract_plan(const z3::model& model, int max_timestep) const {
    Plan plan;

    std::cout << "Extracting plan from Z3 model with " << model.size() << " variable assignments" << std::endl;

    // Use type-safe capability query instead of string comparison
    bool is_parallel = get_parallelism_strategy()->allows_concurrent_actions();
    
    // Iterate through each timestep
    for (int t = 0; t < max_timestep; ++t) {
        if (is_parallel) {
            // Extract and order parallel actions for this timestep
            std::vector<const Action*> parallel_actions = extract_parallel_actions_at_timestep(model, t);
            
            if (!parallel_actions.empty()) {
                std::vector<const Action*> ordered_actions = topologically_sort_actions(parallel_actions);
                for (const Action* action : ordered_actions) {
                    plan.add_action(action);
                }
            }
        } else {
            // Original sequential extraction logic  
            for (const Action& grounded_action : problem_.actions()) {
                z3::expr action_var = variable_factory_.get_action_variable(grounded_action, t);
                z3::expr action_value = model.eval(action_var, true); // Use model completion
                
                if (action_value.is_true()) {
                    plan.add_action(&grounded_action);
                    break; // Only one action in sequential mode
                }
            }
        }
    }
    return plan;
}

std::vector<const Action*> GroundedEncoder::extract_parallel_actions_at_timestep(
    const z3::model& model, int timestep) const {
    
    std::vector<const Action*> parallel_actions;
    
    for (const Action& grounded_action : problem_.actions()) {
        try {
            z3::expr action_var = variable_factory_.get_action_variable(grounded_action, timestep);
            z3::expr action_value = model.eval(action_var, true);
            
            if (action_value.is_true()) {
                parallel_actions.push_back(&grounded_action);
            }
        } catch (const std::exception&) {
            // Skip actions whose variables don't exist
        }
    }
    
    return parallel_actions;
}

std::vector<const Action*> GroundedEncoder::topologically_sort_actions(
    const std::vector<const Action*>& actions) const {

    if (actions.size() <= 1) {
        return actions;
    }

    const ParallelismStrategy* strategy = get_parallelism_strategy();
    const InterferenceAnalysis* analyzer = strategy->get_interference_analyzer();

    return analyzer->topological_sort_actions(actions);
}

} // namespace rantanplan