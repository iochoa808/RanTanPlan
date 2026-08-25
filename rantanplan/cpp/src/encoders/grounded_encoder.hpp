#pragma once

#include "../problem/problem.hpp"
#include "base_encoder.hpp"
#include "grounded_encoding_visitor.hpp"
#include "z3_variable_factory.hpp"
#include "parallelism/parallelism_strategy.hpp"
#include <z3++.h>

#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>

// This class is able to handle the encoding of grounded fluents and actions.
namespace rantanplan {

class GroundedEncoder : public BaseEncoder {
public:
    // [XTS] Which frame-axiom encoding array/set fluents use in encode_frames.
    //   Disequality (default) — (arr^t != arr^{t+1}) -> disjunction(actions), same
    //                            shape as scalars.
    //   Ite                   — total-update ITE chain, ported from branch z3-proves.
    // Selected via --array-frame-mode (see config.hpp / strategy_factory.cpp).
    enum class ArrayFrameMode { Disequality, Ite };

    // Constructor
    GroundedEncoder(const Problem& problem, z3::context& ctx);

    // [XTS] Select the array/set frame-axiom encoding (see ArrayFrameMode above).
    void set_array_frame_mode(ArrayFrameMode mode) { array_frame_mode_ = mode; }

    // [XTS-UnFun] Select the array/set encoding backend (Theory vs UF). Forwards to
    // the variable factory, which is the single source of truth both this class'
    // encode_frames()/encode_non_cumulative_effects() and GroundedEncodingVisitor
    // consult (see Z3VariableFactory::ArrayEncodingMode).
    void set_array_encoding_mode(Z3VariableFactory::ArrayEncodingMode mode) {
        variable_factory_.set_array_encoding_mode(mode);
    }

    // Encoding steps
    std::shared_ptr<z3::expr> encode_initial_state() override;
    std::shared_ptr<z3::expr> encode_actions(int t) override; // Encodes actions layer_idx 
    std::shared_ptr<z3::expr> encode_frames(int t) override; // Encodes frame axioms for layer_idx to layer_idx+1
    std::shared_ptr<z3::expr> encode_goal(int t) override;    // Encodes goal conditions at layer_idx
    std::shared_ptr<z3::expr> encode_parallelism(int t) override; // Encodes parallelism semantics
    std::shared_ptr<z3::expr> encode_symmetries(int t) override;
    std::shared_ptr<z3::expr> encode_state_constraints(int t) override;
    std::shared_ptr<z3::expr> encode_prefix_monotone(int t) override; // Front-loading symmetry breaking
    
    // Strategy management
    void set_parallelism_strategy(std::unique_ptr<ParallelismStrategy> strategy) override;
    std::string get_parallelism_strategy_name() const override;
    
    // Access to variable factory for plan extraction
    Z3VariableFactory& get_variable_factory() override { return variable_factory_; }
    const Z3VariableFactory& get_variable_factory() const override { return variable_factory_; }
    
    // Get access to parallelism strategy for plan extraction
    const ParallelismStrategy* get_parallelism_strategy() const override { return parallelism_strategy_.get(); }
    
    // Encode precondition + effect constraints for a single action at timestep t.
    // Returns nullptr if the action has no effects (nothing to encode).
    std::shared_ptr<z3::expr> encode_single_action(const Action& action, int t);

    // Per-action effect constraints (action → effects), without preconditions.
    // ChainedGroundedEncoder overrides to exclude cumulative effects (those
    // require joint encoding across actions via encode_cumulative_effects).
    // Together with encode_cumulative_effects, these compose encode_actions.
    virtual std::shared_ptr<z3::expr> encode_non_cumulative_effects(const Action& action, int t);

    // Per-timestep joint effect constraints across multiple actions.
    // Base: no-op.  ChainedGroundedEncoder: chain variables (ζ) that link
    // actions modifying the same numeric variable for correct cumulative
    // semantics under exists-step.
    // Together with encode_non_cumulative_effects, these compose encode_actions.
    virtual std::shared_ptr<z3::expr> encode_cumulative_effects(int t);

    // Create action variables for all actions at timestep t without encoding constraints.
    void ensure_action_variables(int t);

    Plan extract_plan(const z3::model& model, int max_timestep) const override;
    
    // Helper functions to convert expressions/effects to Z3 using visitor (implementing base interface)
    z3::expr convert_expr_id_to_z3(ExprID id, int timestep = -1) override;
    z3::expr convert_effect_to_z3(const EffectExpression& effect, int timestep) override;
    
protected:
    
    // Member variables accessible to derived classes
    const Problem& problem_; // The planning problem instance
    z3::context& ctx_;       // Z3 context (shared)
    Z3VariableFactory variable_factory_; // Factory for creating and managing Z3 variables

    GroundedEncodingVisitor grounded_visitor_; // Grounded encoding visitor for individual variables

    // Parallelism strategy
    std::unique_ptr<ParallelismStrategy> parallelism_strategy_;

    // Indices for the frame axioms

    // Map from grounded fluent (ExprID) to vector of (Action*, EffectExpression*). For example:
    // epc_index_[expr_id(at(airplane1, city1))] -> [(move_action*, effect_expr*), (fly_action*, effect_expr*)]
    // where each pair represents an action that can affect the fluent at(airplane1, city1)
    std::unordered_map<ExprID, std::vector<std::pair<const Action*, const EffectExpression*>>> epc_index_;

    // [XTS] Array/set fluents are indexed in epc_index_ too, keyed by the root
    // array/set SV id (not by the write expression's own id) — see build_epc_index().
    // This lets encode_frames build their frame axiom exactly like a scalar's:
    // (arr^t != arr^{t+1}) -> (a1 || (c2 && a2) || ...). array_fluent_ids_ just
    // marks WHICH fluent ids are array/set-typed, so encode_frames knows to
    // always build their axiom, eagerly, even under lazy_frames_enabled_ (see
    // below). FrameAxiomModule no longer excludes them — it doesn't need to:
    // the only strategy that activates it is exists-step, and an array/set
    // problem never reaches an encoder under step semantics (rejected in
    // StrategyFactory::create_encoder).
    std::unordered_set<ExprID> array_fluent_ids_;

    // [XTS] One write record per (action, array/set cell write) — feeds the
    // ITE-chain frame axiom in encode_frames when array_frame_mode_ == Ite
    // (ported from branch z3-proves). Unused when mode == Disequality, but
    // always populated by build_epc_index() so the mode can be switched freely.
    // indices holds the CELL COORDINATES only, outermost-first:
    //   1D write arr[k]         → {k}
    //   2D write arr[i][j]      → {i, j}
    //   N-D write arr[i]...[k]  → {i, ..., k}
    //   whole-array ASSIGN      → {} (empty)
    //   plain set add/remove    → {elem}  (a set's "cell" IS its element)
    //   array-of-sets bins[i]   → {i}     (the element lives in set_elem_id)
    //
    // [XTS-UnFun] set_elem_id keeps an array-of-sets cell mutation's element OUT of
    // `indices`. It used to be appended to `indices` as a trailing pseudo-dimension so
    // Theory mode could treat `bins[src] := SetRemove(item, bins[src])` exactly like an
    // array-of-array-of-bool point write. That worked for Theory but collided with UF,
    // where arity counts ARRAY nesting only (a set stays a leaf holding a native
    // Array(Int,Bool) value — see resolve_elem_sort_at_depth's contract). The record
    // then carried arity+1 entries while a UF cell tuple had exactly arity, and every
    // UF consumer had to un-fold the convention by hand — a clamp whose failure mode was
    // z3::expr_vector::operator[] raising Z3_IOB, which ABORTS THE PROCESS rather than
    // surfacing as an error.
    //
    // Now the two readings are separate fields and each consumer takes the one it means:
    //   - UF   wants cell coordinates      → `indices`      (always exactly arity long)
    //   - Theory wants the full store path → `store_path()` (indices + set element)
    struct ArrayWriteRecord {
        const Action*       action;
        ExprID              cond_id;     // EXPR_NULL if unconditional
        ExprID              base_sv_id;  // SV — the root array/set variable
        std::vector<ExprID> indices;     // [XTS] cell coordinates, outermost-first
        ExprID              val_id;      // explicit value for array writes; EXPR_NULL for set writes
        bool                set_value;   // for set writes: true=add, false=remove
        // [XTS-UnFun] Set element of an ARRAY-OF-SETS cell mutation; EXPR_NULL for every
        // other write shape (including a plain set fluent, whose element is already its
        // cell coordinate in `indices`).
        ExprID              set_elem_id;

        // [XTS-UnFun] The full index path down to the written scalar, i.e. what a Theory
        // store chain has to walk and what identifies a written cell for the duplicate-
        // write check. Equals `indices` unless this is an array-of-sets cell mutation,
        // in which case the set element is appended as the innermost step.
        //
        //   arr[i][j] := v            indices={i,j} set_elem_id=NULL → {i, j}
        //   bag := SetAdd(e, bag)     indices={e}   set_elem_id=NULL → {e}
        //   bins[i] := SetAdd(e, ...) indices={i}   set_elem_id=e    → {i, e}
        //   board := <whole assign>   indices={}    set_elem_id=NULL → {}
        std::vector<ExprID> store_path() const {
            std::vector<ExprID> path = indices;
            if (set_elem_id.valid()) path.push_back(set_elem_id);
            return path;
        }
    };
    // Keyed by base_sv_id (same as the SV(arr)/SV(basket) ExprID).
    std::unordered_map<ExprID, std::vector<ArrayWriteRecord>> array_epc_index_;

    void build_epc_index();

    // =====================================================================
    // [XTS] build_epc_index stages
    //
    // build_epc_index had grown to ~200 lines from a 31-line pre-XTS core. It is
    // really three sequential stages plus two one-line recorders that were inline
    // lambdas; naming them lets build_epc_index itself read as the outline it is.
    // =====================================================================

    // Stage 1: give every grounded fluent an empty epc_index_ entry, and collect the
    // [XTS] array/set fluent ids and IPAR cell SVs that encode_frames keys off.
    void seed_fluent_indices();

    // epc_index_[key] += (action, eff) → the disequality-mode frame axiom (default).
    // Used identically for scalar AND array/set fluents: only the KEY differs for array
    // writes (peeled down to the root array/set SV); the bookkeeping itself — one
    // (action, effect) pair per possible write — is the same for every fluent kind.
    void index_fluent(ExprID key, const Action* action, const EffectExpression* eff_expr);

    // [XTS] array_epc_index_[sv] += record → the ITE-chain frame axiom
    // (--array-frame-mode ite).  idxs={} → whole-fluent replace; adding=true/false →
    // SET_ADD/SET_REMOVE. Built unconditionally alongside index_fluent (regardless of
    // the currently selected mode) since set_array_frame_mode() may be called after
    // build_epc_index() already ran — it runs once, from the constructor.
    // [XTS-UnFun] set_elem defaults to EXPR_NULL: only an array-of-sets cell mutation
    // passes it. Every other write shape keeps its whole index path in `idxs`.
    void record_array_write(const Action* action, ExprID cond, ExprID sv,
                            std::vector<ExprID> idxs, ExprID val, bool adding,
                            ExprID set_elem = EXPR_NULL);

    // Stage 2 (per effect): record the four [XTS] array/set write shapes — N-D cell
    // write, plain-set point write, whole-fluent ASSIGN, and IPAR cell write. Returns
    // true when one of them matched; false means "plain scalar effect", which
    // build_epc_index then indexes directly. Same handler contract as the
    // try_encode_* family above.
    bool try_record_array_effect(const Action& action, const EffectExpression& eff_expr);

    // Stage 3: warn about two unconditional writes from one action to the same literal
    // cell. Diagnostic only — the encoding stays sound either way.
    void warn_duplicate_cell_writes() const;

public:
    const auto& get_epc_index() const { return epc_index_; }
    void set_lazy_frames(bool enabled) { lazy_frames_enabled_ = enabled; }

    /// Extract true action variables at a timestep from a Z3 model.
    std::vector<const Action*> extract_parallel_actions_at_timestep(const z3::model& model, int timestep) const;

protected:

    bool lazy_frames_enabled_ = false;

    // [XTS] Array/set frame-axiom encoding, set via set_array_frame_mode()
    // (default: Disequality). See ArrayFrameMode above and encode_frames().
    ArrayFrameMode array_frame_mode_ = ArrayFrameMode::Disequality;

    // [XTS] Build a nested z3::store for an N-D cell write.
    // indices[from..] are the remaining dimension indices, outermost-first.
    static z3::expr build_store_chain(const z3::expr& arr,
                                      const std::vector<z3::expr>& indices,
                                      size_t from, const z3::expr& val);

    // [XTS-UnFun] Shared write-record folding scaffolding for encode_frames' Ite
    // (Theory) and UF branches. Both branches need the exact same shape — group by
    // firing action, fold each action's own records with last-write-wins, then fold
    // across actions with ite(act_var, chained, update) — and differ only in how one
    // record turns into a value (store()/select() on a whole array term for Theory;
    // an explicit index-match ite() on one cell for UF). See grounded_encoder.cpp for
    // the full rationale and both call sites.
    static void group_records_by_action(
        const std::vector<ArrayWriteRecord>& write_list,
        std::vector<const Action*>& action_order,
        std::unordered_map<const Action*, std::vector<const ArrayWriteRecord*>>& by_action);

    // `resolve_record(chained, rec)` resolves one write record into a new value,
    // given the value accumulated so far from earlier records of the same action
    // (`chained`, starting at `base_value`). Conditional effects are wrapped around
    // the caller's result uniformly here, so `resolve_record` itself never needs to
    // look at rec.cond_id.
    z3::expr fold_array_writes(
        const std::vector<ArrayWriteRecord>& write_list, int t, const z3::expr& base_value,
        const std::function<z3::expr(const z3::expr& chained, const ArrayWriteRecord& rec)>& resolve_record);

    // [XTS-UnFun] UF + Disequality mode's positive-direction write facts (the pointwise
    // analogue of Theory-Disequality's per-action store-chain assertions): one direct
    // point fact per effect, asserted from encode_non_cumulative_effects, with no
    // last-write-wins chaining (unlike array_intermediates above). This is sound
    // because two point facts about *different* arguments of the same uninterpreted
    // function never conflict or need composing — unlike Array theory, there is no
    // extensionality to entangle them. Two writes to the exact same
    // literal cell from the *same* action would conflict (make the action unfireable),
    // but that pattern is already a documented domain-modeling error elsewhere (PDDL-XTS
    // test X_double_write_same_cell) and never arises from forall-expansion, whose
    // indices are always distinct literals.
    z3::expr build_uf_point_write_fact(ExprID base_sv, const std::vector<ExprID>& indices,
                                        const z3::expr& val, ExprID cond_id, int t);

    // [XTS-UnFun] UF + Disequality mode's positive-direction fact for a whole-array/set
    // ASSIGN effect ("bag := <expr>") — the one write shape that still needs domain
    // enumeration, since it inherently touches every cell (no way to name "everything"
    // pointwise otherwise).
    z3::expr build_uf_whole_assign_fact(ExprID base_sv, const EffectExpression& effect, int t);

    // =====================================================================
    // [XTS] Effect-shape handlers, extracted from encode_non_cumulative_effects
    //
    // encode_non_cumulative_effects had grown to ~190 lines from a 13-line pre-XTS
    // core, because each array/set effect shape was inlined into the effect loop as
    // an `if (...) { ...; continue; }` block. Each is now its own handler, and the
    // loop reads as the dispatch table it always was:
    //
    //   if (try_encode_array_write_effect(...))      continue;
    //   if (try_encode_uf_whole_fluent_effect(...))  continue;
    //   effect_exprs.push_back(convert_effect_to_z3(effect, t));  // scalar fallback
    //
    // CONTRACT for every try_*: return true iff the effect was fully handled and the
    // caller should move on. Returning false means "not my shape" and MUST leave
    // effect_exprs and intermediates untouched, so the next handler sees a clean
    //
    // `intermediates` is the per-action chained-array map owned by the caller; only
    // the Theory-mode paths write to it. Under UF nothing is ever inserted, which is
    // what makes the caller's flush loop a no-op in that mode.
    // =====================================================================

    // [XTS-UnFun] True when UF+Ite mode owns this effect entirely via encode_frames'
    // pointwise fold, so the effect loop must emit nothing for it. Covers all three
    // array/set effect shapes (ARRAY_WRITE, plain array/set SV).
    bool uf_ite_defers_effect(ExprID fluent_id) const;

    // [XTS] Current in-flight value of an array/set SV inside one action's effect
    // sequence: the chained intermediate if an earlier effect of this same action
    // already wrote it, otherwise the fluent's plain value at `t`.
    z3::expr array_intermediate_or_current(
        ExprID sv_id, int t, const std::unordered_map<ExprID, z3::expr>& intermediates);

    // [XTS] N-D cell write — ARRAY_WRITE(ARRAY_READ(board,1),2) := v. Also covers the
    // array-of-sets cell mutation bins[src] := SetRemove(item, bins[src]).
    bool try_encode_array_write_effect(const EffectExpression& effect, int t, bool uf_mode,
                                       z3::expr_vector& effect_exprs,
                                       std::unordered_map<ExprID, z3::expr>& intermediates);

    // [XTS-UnFun] UF only: the effect fluent is itself an array/set SV, so this is
    // either a SET_ADD/REMOVE point write or a whole-fluent ASSIGN. Theory mode
    // returns false here and lets convert_effect_to_z3 handle both, unchanged.
    bool try_encode_uf_whole_fluent_effect(const EffectExpression& effect, int t, bool uf_mode,
                                           z3::expr_vector& effect_exprs);

    int layers_encoded_ = -1; // Tracks the highest layer for which transitions are encoded

private:
    std::vector<const Action*> topologically_sort_actions(const std::vector<const Action*>& actions) const;

    // Cache of (ground fluent ExprID, lo, hi) for scalar fluents with declared type bounds.
    // Built lazily on first use; populated by build_type_bounds_cache().
    struct TypeBound { ExprID fluent_id; int64_t lo; int64_t hi; };
    mutable std::vector<TypeBound> type_bounds_cache_;

    // Cache of (ground array fluent ExprID, size, lo, hi) for 1-D array fluents
    // whose element type is a bounded integer.  Per-cell bounds are added via
    // select(arr_t, i) >= lo / <= hi in encode_state_constraints.
    struct ArrayElemTypeBound { ExprID fluent_id; int64_t size; int64_t lo; int64_t hi; };
    mutable std::vector<ArrayElemTypeBound> array_elem_bounds_cache_;

    mutable bool type_bounds_built_ = false;
    void build_type_bounds_cache() const;

    // Emit the declared type-range constraints (both caches above) at timestep t.
    // Shared by encode_state_constraints (t < h) and encode_type_bounds (goal state h).
    void append_type_bound_constraints(int t, z3::expr_vector& conjuncts);

public:
    // [XTS] See BaseEncoder::encode_type_bounds. Returns nullptr when the problem
    // declares no bounded-int fluent/array-element types.
    std::shared_ptr<z3::expr> encode_type_bounds(int t) override;
};

} // namespace rantanplan
