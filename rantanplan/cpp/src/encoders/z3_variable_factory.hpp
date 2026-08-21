#pragma once

#include "../problem/fluent.hpp"
#include "../problem/action.hpp"
#include <z3++.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>

namespace rantanplan {

class Problem;  // forward declaration

/**
 * @brief Factory class responsible for creating and managing Z3 variables
 *
 * This factory encapsulates all Z3 variable creation logic and provides
 * type-safe methods for creating variables based on fluent types.
 * It maintains variable storage and caching to ensure consistency
 * across the encoding process.
 *
 * Key responsibilities:
 * - Variable naming (fluents and actions)
 * - Variable creation with proper types
 * - Variable caching and lifecycle management
 * - Timestep-aware variable management
 *
 * ---------------------------------------------------------------------------
 * Layout of this class (mirrored one-for-one in z3_variable_factory.cpp):
 *
 *   [0] Backend selection      — which theory backs array/set fluents
 *   [1] Construction & wiring  — context, Problem, numeric sort policy
 *   [2] COMMON  variable creation      — bool / int / real / object / symbol
 *   [3] COMMON  fluent & action vars   — cached, timestep-indexed accessors
 *   [4] COMMON  naming                 — var-name construction
 *   [5] COMMON  reverse lookup         — Z3 var  ->  fluent / action
 *   [6] COMMON  enumeration            — bulk access for propagators
 *   [7] COMMON  numeric constants      — Int/Real literals
 *   [8] COMMON  timestep management
 *   [9] SHARED  sort resolution        — used by BOTH backends
 *  [10] THEORY  array/set variables    — Z3 Array sort, select/store
 *  [11] UF      uninterpreted functions— one func_decl per (fluent, timestep)
 *
 * "COMMON" = encoding-agnostic, "THEORY"/"UF" = only reachable under the
 * corresponding ArrayEncodingMode. The two backends meet in exactly two
 * places: section [9] (sort resolution) and create_new_fluent_variable()
 * in section [3], which dispatches a bare array/set fluent to the Theory
 * representation. Under UF, array/set fluents go through get_array_uf()
 * ([11]) instead and never materialise a single z3::expr "value".
 * ---------------------------------------------------------------------------
 */
class Z3VariableFactory {
public:
    // =====================================================================
    // [0] Backend selection
    // =====================================================================

    // [XTS-UnFun] Which Z3 theory backs array/set fluents.
    //   UF (default) — one uninterpreted function per (fluent, timestep); reads are
    //                  direct function application, writes/frame axioms are asserted
    //                  pointwise over the fluent's enumerated static domain.
    //   Theory       — Z3 Array sort, select/store
    enum class ArrayEncodingMode { Theory, UF };

    // [XTS-UnFun] Select the array/set encoding backend (see ArrayEncodingMode above).
    void set_array_encoding_mode(ArrayEncodingMode mode) { array_encoding_mode_ = mode; }
    ArrayEncodingMode array_encoding_mode() const { return array_encoding_mode_; }

    // [XTS-UnFun] Gates every array/set code path in the encoder and visitor.
    bool uf_mode() const { return array_encoding_mode_ == ArrayEncodingMode::UF; }

    // =====================================================================
    // [1] Construction & wiring
    // =====================================================================

    explicit Z3VariableFactory(z3::context& ctx);

    /// Access the underlying Z3 context.
    z3::context& ctx() const { return ctx_; }

    /// Set the problem pointer for integer mode detection.
    void set_problem(const Problem* p) { problem_ = p; }

    // =====================================================================
    // [2] COMMON — primitive variable creation
    // (create new variables without caching)
    // =====================================================================

    z3::expr create_bool_variable(const std::string& name);
    z3::expr create_int_variable(const std::string& name);
    z3::expr create_real_variable(const std::string& name);
    z3::expr create_object_variable(const std::string& name);

    // Symbol-based variable creation (for visit_symbol)
    z3::expr create_symbol_variable(const std::string& name, const Type* type);

    // =====================================================================
    // [3] COMMON — fluent & action variables (cached, timestep-indexed)
    // =====================================================================

    // High-level variable management with caching
    const z3::expr& get_fluent_variable(const Fluent& fluent, int timestep);
    const z3::expr& get_action_variable(const Action& action, int timestep);

    // Const versions for read-only access (will lookup existing variables)
    const z3::expr& get_fluent_variable(const Fluent& fluent, int timestep) const;
    const z3::expr& get_action_variable(const Action& action, int timestep) const;

    // =====================================================================
    // [4] COMMON — variable naming
    // =====================================================================

    std::string get_fluent_var_name(const Fluent& fluent) const;
    std::string get_fluent_var_name(const Fluent& fluent, int timestep) const;
    std::string get_action_var_name(const Action& action) const;
    std::string get_action_var_name(const Action& action, int timestep) const;

    // =====================================================================
    // [5] COMMON — reverse lookup (Z3 variable -> fluent / action)
    // =====================================================================

    std::optional<std::pair<const Fluent&, int>> get_fluent_from_variable(const z3::expr& var) const;
    std::optional<std::pair<const Action&, int>> get_action_from_variable(const z3::expr& var) const;
    bool is_fluent_variable(const z3::expr& var) const;
    bool is_action_variable(const z3::expr& var) const;

    // =====================================================================
    // [6] COMMON — variable enumeration (for propagators)
    // =====================================================================

    std::vector<std::shared_ptr<z3::expr>> get_all_fluent_variables(int timestep) const;
    std::vector<std::shared_ptr<z3::expr>> get_all_action_variables(int timestep) const;
    std::vector<std::pair<std::shared_ptr<z3::expr>, int>> get_all_fluent_variables() const;
    std::vector<std::pair<std::shared_ptr<z3::expr>, int>> get_all_action_variables() const;

    // =====================================================================
    // [7] COMMON — numeric constants (Int or Real depending on all_integer mode)
    // =====================================================================

    z3::expr make_numeric_val(int64_t v) const;
    z3::expr make_numeric_val(double v) const;
    z3::expr make_zero() const;

    // =====================================================================
    // [8] COMMON — timestep management queries
    // =====================================================================

    int get_max_timestep() const;
    bool has_variables_for_timestep(int timestep) const;

    // =====================================================================
    // [9] SHARED — element-sort resolution (used by BOTH backends)
    //
    // Theory uses these to pick the Array sort's range; UF uses them to pick
    // the uninterpreted function's range. Same type dispatch, two consumers.
    // =====================================================================

    // [XTS-UnFun] Resolve the Z3 sort of an array/set fluent's element after descending
    // `depth` array dimensions from its declared value type `t` (depth = number of
    // ARRAY_READ indices already consumed / the UF function's arity for array fluents).
    // depth=0 returns resolve_elem_sort_for_type(t) itself (used for sets, where the
    // fluent's own value type IS the Bool leaf via is_set()).
    z3::sort resolve_elem_sort_at_depth(const Type* t, int depth) const;

    // =====================================================================
    // [10] THEORY — array/set variables (Z3 Array sort)
    // =====================================================================

    // [XTS-Arrays]
    // Array/set variable creation.
    // Arrays are Array(Int, elem_sort); sets are Array(Int, Bool).
    // The caller resolves elem_sort via resolve_elem_sort().
    z3::expr create_array_variable(const std::string& name, z3::sort elem_sort);
    z3::expr create_set_variable(const std::string& name);

    // =====================================================================
    // [11] UF — uninterpreted-function backend for array/set fluents
    // =====================================================================

    // [XTS-UnFun] Return (creating on first access) the uninterpreted function standing
    // in for an array/set fluent's contents at `timestep`, under ArrayEncodingMode::UF.
    // Signature: Int^arity -> elem_sort — one Int argument per array dimension (arity=1
    // for sets and 1-D arrays). Cached the same way get_fluent_variable caches z3::expr,
    // just keyed by func_decl instead since a bare array/set fluent has no single "value".
    const z3::func_decl& get_array_uf(const Fluent& fluent, int timestep,
                                      unsigned arity, z3::sort elem_sort);

    // [XTS-UnFun] Turn one enumerate_array_domain() index tuple into the argument vector
    // for a UF function application. Pure plumbing (int64 -> Z3 Int literal), but it was
    // open-coded identically at every pointwise call site — initial state, whole-array
    // assign, both frame-axiom branches, and per-cell value evaluation.
    //
    //   cell_args({2})      ->  (2)        e.g. fn(2)      for board[2]
    //   cell_args({1, 0})   ->  (1, 0)     e.g. fn(1, 0)   for grid[1][0]
    //   cell_args({})       ->  ()         (degenerate; no caller does this today)
    z3::expr_vector cell_args(const std::vector<int64_t>& cell) const;

    // [XTS-UnFun] Bundles the two values every UF call site needs together: a
    // fluent's UF function arity (1 for sets; array nesting depth for arrays) and
    // its elem_sort. Both fall out of a single descend_array_levels() walk
    // (array_domain_utils.hpp) — the same descent leaf_element_type performs. Saves
    // callers from re-deriving arity via domain[0].size() or repeating the is_set() case.
    std::pair<unsigned, z3::sort> resolve_uf_shape(const Type* t) const;

private:
    // =====================================================================
    // Private helpers — mirroring the public sectioning above
    // =====================================================================

    // [1] Wiring: latch Problem::all_integer() on first use so numeric_is_int()
    // gives all variables a consistent Int/Real sort, avoiding silent sort mismatches.
    bool numeric_is_int() const;

    // [3] Fluent/action variable management
    void ensure_timestep_capacity(int timestep);
    z3::expr create_new_fluent_variable(const Fluent& fluent, const std::string& var_name);

    // [9] SHARED sort resolution
    // Resolve the Z3 sort for a type given by name (used for array element types).
    z3::sort resolve_elem_sort(const std::string& type_name) const;

    // [XTS-UnFun] Same dispatch as resolve_elem_sort, but takes a resolved Type* directly
    // instead of re-parsing a type name via find_type(). Factored out so the callers that
    // walk a chain of Type* (resolve_elem_sort_at_depth, resolve_uf_shape) can hand over
    // the Type* they already landed on rather than round-tripping through its name.
    z3::sort resolve_elem_sort_for_type(const Type* t) const;

    // =====================================================================
    // Data members
    // =====================================================================

    // --- [1] Wiring ------------------------------------------------------
    z3::context& ctx_;
    const Problem* problem_ = nullptr;

    mutable bool all_integer_latched_ = false;
    mutable bool all_integer_cache_   = false;

    // --- [0] Backend selection -------------------------------------------
    // [XTS] Selected via --array-encoding (see config.hpp / strategy_factory.cpp).
    // Mirrors config.hpp's "uf" default so a factory built outside the normal
    // pipeline still gets the same backend the CLI would have given it.
    ArrayEncodingMode array_encoding_mode_ = ArrayEncodingMode::UF;

    // --- [3]/[5] COMMON storage ------------------------------------------
    // Storage for SMT variables - outer vector is indexed by timestep
    // state_vars_[0]["at_airplane1_city1"] -> z3::expr(bool variable for fluent at timestep 0)
    std::vector<std::unordered_map<std::string, std::shared_ptr<z3::expr>>> state_vars_;

    // action_vars_[0]["move_airplane1_city1_city2"] -> z3::expr(bool variable for action at timestep 0)
    std::vector<std::unordered_map<std::string, std::shared_ptr<z3::expr>>> action_vars_;

    // Reverse mappings from variable names to fluents/actions and timesteps
    std::unordered_map<std::string, std::pair<std::shared_ptr<Fluent>, int>> state_var_name_to_fluent_;
    std::unordered_map<std::string, std::pair<std::shared_ptr<Action>, int>> action_var_name_to_action_;

    // --- [11] UF storage --------------------------------------------------
    // [XTS-UnFun] uf_fns_[t]["read_<fluent_var_name>"] -> the uninterpreted function
    // standing in for that array/set fluent's contents at timestep t. Only populated
    // when array_encoding_mode_ == UF; parallels state_vars_ but for func_decl instead
    // of expr, since a bare array/set fluent no longer has a single Z3 "value" to cache.
    std::vector<std::unordered_map<std::string, std::shared_ptr<z3::func_decl>>> uf_fns_;
};

} // namespace rantanplan
