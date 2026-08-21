#include "z3_variable_factory.hpp"
#include "array_domain_utils.hpp"  // [XTS-UnFun] descend_array_levels
#include "../problem/action.hpp"
#include "../problem/problem.hpp"
#include <iostream>

// ===========================================================================
// Section order mirrors z3_variable_factory.hpp one-for-one:
//   [1] Construction & wiring
//   [2] COMMON  primitive variable creation
//   [3] COMMON  fluent & action variables (cached, timestep-indexed)
//   [4] COMMON  variable naming
//   [5] COMMON  reverse lookup
//   [6] COMMON  variable enumeration
//   [7] COMMON  numeric constants
//   [8] COMMON  timestep management
//   [9] SHARED  element-sort resolution (both backends)
//  [10] THEORY  array/set variables (Z3 Array sort)
//  [11] UF      uninterpreted-function backend
// ===========================================================================

namespace rantanplan {

// ===========================================================================
// [1] Construction & wiring
// ===========================================================================

Z3VariableFactory::Z3VariableFactory(z3::context& ctx)
    : ctx_(ctx) {
    state_vars_.clear();
    action_vars_.clear();
    state_var_name_to_fluent_.clear();
    action_var_name_to_action_.clear();
}

// [XTS] See header: Problem::all_integer() latched at first consult, so every
// numeric sort decision this factory ever makes agrees with the first one.
bool Z3VariableFactory::numeric_is_int() const {
    if (!all_integer_latched_) {
        all_integer_cache_ = problem_ && problem_->all_integer();
        all_integer_latched_ = true;
    }
    return all_integer_cache_;
}

// ===========================================================================
// [2] COMMON — primitive variable creation
// ===========================================================================

z3::expr Z3VariableFactory::create_bool_variable(const std::string& name) {
    return ctx_.bool_const(name.c_str());
}

z3::expr Z3VariableFactory::create_int_variable(const std::string& name) {
    return ctx_.int_const(name.c_str());
}

z3::expr Z3VariableFactory::create_real_variable(const std::string& name) {
    return ctx_.real_const(name.c_str());
}

z3::expr Z3VariableFactory::create_object_variable(const std::string& name) {
    // Object fluents are encoded as numeric values (each object maps to its global index).
    // They must use the same Z3 sort as numeric fluents to avoid Int/Real mixing,
    // which would crash Z3 when a single-sort logic hint (e.g. QF_LRA) is active.
    // Using Real sort when all_integer is false is safe because object values are always
    // integer-valued (assigned via equality with concrete object constants).
    return numeric_is_int()
        ? ctx_.int_const(name.c_str())
        : ctx_.real_const(name.c_str());
}

z3::expr Z3VariableFactory::create_symbol_variable(const std::string& name, const Type* type) {
    if (!type) {
        // For unknown types, default to integer
        return create_int_variable(name);
    }

    if (type->is_bool()) {
        return create_bool_variable(name);
    } else if (type->is_int()) {
        return create_int_variable(name);
    } else if (type->is_real()) {
        return numeric_is_int()
            ? create_int_variable(name) : create_real_variable(name);
    } else if (type->is_object()) {
        // Object types follow the same sort policy as numeric fluents (see create_object_variable).
        return create_object_variable(name);
    } else {
        // For unknown types, default to integer
        return create_int_variable(name);
    }
}

// ===========================================================================
// [3] COMMON — fluent & action variables (cached, timestep-indexed)
// ===========================================================================

// Return the Z3 variable for (fluent, timestep), creating it on first access.
// Cache key is the full name string (e.g. "at_robot_cityA_3") — Z3 also deduplicates
// by name internally, so the same name always yields the same Z3 constant.
// The reverse map (state_var_name_to_fluent_) lets plan extraction look up which
// fluent a Z3 model variable corresponds to.
const z3::expr& Z3VariableFactory::get_fluent_variable(const Fluent& fluent, int timestep) {
    std::string var_name = get_fluent_var_name(fluent, timestep);

    ensure_timestep_capacity(timestep);

    auto& timestep_vars = state_vars_[timestep];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        z3::expr new_var = create_new_fluent_variable(fluent, var_name);
        timestep_vars[var_name] = std::make_shared<z3::expr>(new_var);
        state_var_name_to_fluent_[var_name] = {std::make_shared<Fluent>(fluent), timestep};
        return *(timestep_vars[var_name]);
    }
    return *(it->second);
}

// Return (or create) the boolean Z3 variable for (action, timestep).
// Action vars are always Bool — "move_robot_cityA_cityB_2" is true iff that
// grounded action fires at step 2. Same caching/reverse-map pattern as fluents.
const z3::expr& Z3VariableFactory::get_action_variable(const Action& action, int timestep) {
    std::string var_name = get_action_var_name(action, timestep);

    ensure_timestep_capacity(timestep);

    auto& timestep_vars = action_vars_[timestep];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        z3::expr new_var = create_bool_variable(var_name);
        timestep_vars[var_name] = std::make_shared<z3::expr>(new_var);
        action_var_name_to_action_[var_name] = {std::make_shared<Action>(action), timestep};
        return *(timestep_vars[var_name]);
    }
    return *(it->second);
}

// Const versions for read-only access
const z3::expr& Z3VariableFactory::get_fluent_variable(const Fluent& fluent, int timestep) const {
    std::string var_name = get_fluent_var_name(fluent, timestep);

    if (timestep >= static_cast<int>(state_vars_.size())) {
        throw std::runtime_error("Timestep " + std::to_string(timestep) + " not allocated yet");
    }

    const auto& timestep_vars = state_vars_[timestep];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        throw std::runtime_error("Fluent variable " + var_name + " not found for timestep " + std::to_string(timestep));
    }
    return *(it->second);
}

const z3::expr& Z3VariableFactory::get_action_variable(const Action& action, int timestep) const {
    std::string var_name = get_action_var_name(action, timestep);

    if (timestep >= static_cast<int>(action_vars_.size())) {
        throw std::runtime_error("Timestep " + std::to_string(timestep) + " not allocated yet");
    }

    const auto& timestep_vars = action_vars_[timestep];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        throw std::runtime_error("Action variable " + var_name + " not found for timestep " + std::to_string(timestep));
    }
    return *(it->second);
}

void Z3VariableFactory::ensure_timestep_capacity(int timestep) {
    while (static_cast<int>(state_vars_.size()) <= timestep) {
        state_vars_.emplace_back();
    }
    while (static_cast<int>(action_vars_.size()) <= timestep) {
        action_vars_.emplace_back();
    }
    // [XTS-UnFun] uf_fns_ grows alongside state_vars_/action_vars_ so get_array_uf()
    // can index it the same way regardless of which timestep is touched first.
    while (static_cast<int>(uf_fns_.size()) <= timestep) {
        uf_fns_.emplace_back();
    }
}

// This is the one place where the COMMON path hands off to a backend-specific
// representation: the array/set branches build the Theory ([10]) variables.
// Under ArrayEncodingMode::UF, array/set fluents are reached via get_array_uf()
// ([11]) instead and never take these branches.
z3::expr Z3VariableFactory::create_new_fluent_variable(const Fluent& fluent, const std::string& var_name) {
    const Type* vt = fluent.value_type();

    if (fluent.is_bool_fluent()) {
        return create_bool_variable(var_name);
    } else if (fluent.is_int_fluent() || fluent.is_real_fluent()) {
        return numeric_is_int()
            ? create_int_variable(var_name) : create_real_variable(var_name);
    } else if (vt && vt->is_array()) {
        z3::sort elem_sort = resolve_elem_sort(vt->array_element_type_name()); // [XTS]
        return create_array_variable(var_name, elem_sort);                     // [XTS]
    } else if (vt && vt->is_set()) {
        return create_set_variable(var_name);                                  // [XTS]
    } else if (fluent.is_object_fluent()) {
        return create_object_variable(var_name);                               // [XTS]
    } else {
        // For unknown types, default to real (safer than integer for mixed arithmetic)
        std::cerr << "Warning: Unknown fluent type for " << var_name << ", defaulting to real" << std::endl;
        return create_real_variable(var_name);
    }
}

// ===========================================================================
// [4] COMMON — variable naming
// ===========================================================================

// Variable naming: fluent name + underscore-joined grounded param values (no timestep).
// Example: at(robot, cityA)  →  "at_robot_cityA"
// The timestep overload appends "_<t>": at(robot, cityA) t=3  →  "at_robot_cityA_3"
std::string Z3VariableFactory::get_fluent_var_name(const Fluent& fluent) const {
    std::string name = fluent.name();
    for (const auto& param : fluent.parameters()) {
        name += "_" + param.name();
    }
    return name;
}

std::string Z3VariableFactory::get_fluent_var_name(const Fluent& fluent, int timestep) const {
    return get_fluent_var_name(fluent) + "_" + std::to_string(timestep);
}

std::string Z3VariableFactory::get_action_var_name(const Action& action) const {
    std::string name = action.name();
    // Add parameter values from the action's embedded parameters
    for (const auto& param : action.parameters()) {
        name += "_" + param.name();
    }
    return name;
}

std::string Z3VariableFactory::get_action_var_name(const Action& action, int timestep) const {
    return get_action_var_name(action) + "_" + std::to_string(timestep);
}

// ===========================================================================
// [5] COMMON — reverse lookup (Z3 variable -> fluent / action)
// ===========================================================================

std::optional<std::pair<const Fluent&, int>> Z3VariableFactory::get_fluent_from_variable(const z3::expr& var) const {
    std::string var_name = var.decl().name().str();
    auto it = state_var_name_to_fluent_.find(var_name);
    if (it != state_var_name_to_fluent_.end()) {
        return std::make_pair(std::cref(*(it->second.first)), it->second.second);
    }
    return std::nullopt;
}

std::optional<std::pair<const Action&, int>> Z3VariableFactory::get_action_from_variable(const z3::expr& var) const {
    std::string var_name = var.decl().name().str();
    auto it = action_var_name_to_action_.find(var_name);
    if (it != action_var_name_to_action_.end()) {
        return std::make_pair(std::cref(*(it->second.first)), it->second.second);
    }
    return std::nullopt;
}

bool Z3VariableFactory::is_fluent_variable(const z3::expr& var) const {
    std::string var_name = var.decl().name().str();
    return state_var_name_to_fluent_.find(var_name) != state_var_name_to_fluent_.end();
}

bool Z3VariableFactory::is_action_variable(const z3::expr& var) const {
    std::string var_name = var.decl().name().str();
    return action_var_name_to_action_.find(var_name) != action_var_name_to_action_.end();
}

// ===========================================================================
// [6] COMMON — variable enumeration (for propagators)
// ===========================================================================

std::vector<std::shared_ptr<z3::expr>> Z3VariableFactory::get_all_fluent_variables(int timestep) const {
    std::vector<std::shared_ptr<z3::expr>> variables;

    if (timestep >= static_cast<int>(state_vars_.size())) {
        return variables; // Empty vector if timestep not allocated
    }

    const auto& timestep_vars = state_vars_[timestep];
    variables.reserve(timestep_vars.size());

    for (const auto& pair : timestep_vars) {
        variables.push_back(pair.second);
    }

    return variables;
}

std::vector<std::shared_ptr<z3::expr>> Z3VariableFactory::get_all_action_variables(int timestep) const {
    std::vector<std::shared_ptr<z3::expr>> variables;

    if (timestep >= static_cast<int>(action_vars_.size())) {
        return variables; // Empty vector if timestep not allocated
    }

    const auto& timestep_vars = action_vars_[timestep];
    variables.reserve(timestep_vars.size());

    for (const auto& pair : timestep_vars) {
        variables.push_back(pair.second);
    }

    return variables;
}

std::vector<std::pair<std::shared_ptr<z3::expr>, int>> Z3VariableFactory::get_all_fluent_variables() const {
    std::vector<std::pair<std::shared_ptr<z3::expr>, int>> variables;

    for (int timestep = 0; timestep < static_cast<int>(state_vars_.size()); ++timestep) {
        const auto& timestep_vars = state_vars_[timestep];
        for (const auto& pair : timestep_vars) {
            variables.emplace_back(pair.second, timestep);
        }
    }

    return variables;
}

std::vector<std::pair<std::shared_ptr<z3::expr>, int>> Z3VariableFactory::get_all_action_variables() const {
    std::vector<std::pair<std::shared_ptr<z3::expr>, int>> variables;

    for (int timestep = 0; timestep < static_cast<int>(action_vars_.size()); ++timestep) {
        const auto& timestep_vars = action_vars_[timestep];
        for (const auto& pair : timestep_vars) {
            variables.emplace_back(pair.second, timestep);
        }
    }

    return variables;
}

// ===========================================================================
// [7] COMMON — numeric constants (Int or Real depending on all_integer mode)
// ===========================================================================

z3::expr Z3VariableFactory::make_numeric_val(int64_t v) const {
    return numeric_is_int()
        ? ctx_.int_val(static_cast<int64_t>(v))
        : ctx_.real_val(static_cast<int>(v));
}

z3::expr Z3VariableFactory::make_numeric_val(double v) const {
    if (numeric_is_int()) {
        return ctx_.int_val(static_cast<int64_t>(v));
    }
    return ctx_.real_val(std::to_string(v).c_str());
}

z3::expr Z3VariableFactory::make_zero() const {
    return make_numeric_val(static_cast<int64_t>(0));
}

// ===========================================================================
// [8] COMMON — timestep management queries
// ===========================================================================

int Z3VariableFactory::get_max_timestep() const {
    int max_state = static_cast<int>(state_vars_.size()) - 1;
    int max_action = static_cast<int>(action_vars_.size()) - 1;
    return std::max(max_state, max_action);
}

bool Z3VariableFactory::has_variables_for_timestep(int timestep) const {
    bool has_state_vars = timestep < static_cast<int>(state_vars_.size()) && !state_vars_[timestep].empty();
    bool has_action_vars = timestep < static_cast<int>(action_vars_.size()) && !action_vars_[timestep].empty();
    return has_state_vars || has_action_vars;
}

// ===========================================================================
// [9] SHARED — element-sort resolution (used by BOTH backends)
//
// Theory consumes these to pick an Array sort's range ([10]); UF consumes them
// to pick an uninterpreted function's range ([11]). Same type dispatch, both
// backends. resolve_elem_sort / resolve_elem_sort_for_type are the primitives;
// resolve_elem_sort_at_depth adds dimension descent on top of them.
// ===========================================================================

// [XTS] Resolve the Z3 sort for an array or set element type.
// Recursion handles nested arrays so N-D arrays get the right nested sort.
//   up:bool               → Bool
//   up:real (mixed mode)  → Real
//   set{T}                → Array(Int, Bool)
//   array[N, array[N, up:int]] → Array(Int, Array(Int, Int))   (2D case)
//   up:int / bounded_int / object → Int  (objects are global integer indices)
z3::sort Z3VariableFactory::resolve_elem_sort(const std::string& type_name) const {
    if (problem_) {
        const Type* t = problem_->find_type(type_name);
        if (t) return resolve_elem_sort_for_type(t);
    }
    return ctx_.int_sort();
}

// [XTS-UnFun] Same dispatch as resolve_elem_sort, given an already-resolved Type*.
// resolve_elem_sort(name) is now a thin find_type() + delegate to this.
z3::sort Z3VariableFactory::resolve_elem_sort_for_type(const Type* t) const {
    if (!t) return ctx_.int_sort();
    if (t->is_bool()) return ctx_.bool_sort();
    if (t->is_real() && !numeric_is_int()) return ctx_.real_sort();
    if (t->is_array()) {
        // Nested array (e.g. up:array[4, up:array[4, up:int]]): recurse.
        z3::sort inner = resolve_elem_sort(t->array_element_type_name());
        return ctx_.array_sort(ctx_.int_sort(), inner);
    }
    if (t->is_set()) {
        return ctx_.array_sort(ctx_.int_sort(), ctx_.bool_sort()); // [XTS]
    }
    // int, bounded_int, object types: all use Int sort.
    // Object elements are encoded as global integer indices throughout.
    return ctx_.int_sort();
}

// [XTS-UnFun] Descend `depth` array dimensions from `t` (a fluent's declared value
// type) and return the Z3 sort of what's left. Used to compute a UF array function's
// range sort: depth = arity = number of index arguments the function takes.
//   depth=0, t=set{T}                        -> Bool               (elem_sort of a set)
//   depth=1, t=array[4,up:int]                -> Int                (1-D array cell)
//   depth=2, t=array[4,array[4,up:int]]       -> Int                (2-D array cell)
//   depth=1, t=array[4,array[4,up:int]]       -> Array(Int,Int)     (one row; not used
//     by the UF path itself — UF always consumes the full declared depth — but kept
//     general in case a partial read is ever needed).
// Note: if the leaf element type is itself a set (array-of-sets), depth stops at the
// array levels and resolve_elem_sort_for_type returns that leaf's native Array(Int,Bool)
// sort — UF mode only replaces the outer numeric-indexed array dimensions, not a
// per-cell set identity.
z3::sort Z3VariableFactory::resolve_elem_sort_at_depth(const Type* t, int depth) const {
    // [XTS-UnFun] A set has no array dimensions to descend — its UF function's
    // arity is 1 (the element argument), but that 1 isn't an array-nesting level,
    // so the loop below must not run for it. Without this, depth=1 (what every
    // caller actually passes for a set fluent's own arity) falls through to
    // resolve_elem_sort_for_type(t), which sees is_set() and returns the set's
    // native Array(Int,Bool) Theory sort — silently wrong for a UF elem_sort.
    // Handles depth<=1 to also match this function's own depth=0 contract above.
    if (t && t->is_set() && depth <= 1) return ctx_.bool_sort();

    // Shared array descent — see array_domain_utils.hpp. It stops early on a non-array
    // element type, which is the same defensive break the hand-rolled loop had (it should
    // not happen for a well-formed arity). Clamp: a negative depth means "no descent"
    // here, not descend_array_levels' "run to the leaf".
    const int levels = depth > 0 ? depth : 0;
    return resolve_elem_sort_for_type(descend_array_levels(t, problem_, levels).second);
}

// ===========================================================================
// [10] THEORY — array/set variables (Z3 Array sort)
// ===========================================================================

// [XTS] Array variable: Z3 Array(Int, ElemSort) where ElemSort comes from resolve_elem_sort.
z3::expr Z3VariableFactory::create_array_variable(const std::string& name, z3::sort elem_sort) {
    z3::sort arr_sort = ctx_.array_sort(ctx_.int_sort(), elem_sort);
    return ctx_.constant(name.c_str(), arr_sort);
}

// [XTS] Set variable: Z3 Array(Int, Bool) — membership queried by element's integer index.
z3::expr Z3VariableFactory::create_set_variable(const std::string& name) {
    z3::sort set_sort = ctx_.array_sort(ctx_.int_sort(), ctx_.bool_sort());
    return ctx_.constant(name.c_str(), set_sort);
}

// ===========================================================================
// [11] UF — uninterpreted-function backend for array/set fluents
//
// The UF analogue of section [3]: get_array_uf caches one func_decl per
// (fluent, timestep) exactly as get_fluent_variable caches one z3::expr, and
// resolve_uf_shape is the UF analogue of the elem_sort lookup [10] does inline.
// ===========================================================================

// [XTS-UnFun] See header for the full contract. Cache key mirrors get_fluent_variable's
// naming ("read_" prefix keeps it visually and namespace-distinct from any plain
// Bool/Int constant that might otherwise share the fluent's base name).
const z3::func_decl& Z3VariableFactory::get_array_uf(const Fluent& fluent, int timestep,
                                                     unsigned arity, z3::sort elem_sort) {
    std::string var_name = "read_" + get_fluent_var_name(fluent, timestep);

    ensure_timestep_capacity(timestep);

    auto& timestep_fns = uf_fns_[timestep];
    auto it = timestep_fns.find(var_name);
    if (it != timestep_fns.end()) return *(it->second);

    z3::sort_vector domain(ctx_);
    for (unsigned i = 0; i < arity; ++i) domain.push_back(ctx_.int_sort());
    z3::func_decl fn = ctx_.function(var_name.c_str(), domain, elem_sort);
    auto stored = std::make_shared<z3::func_decl>(fn);
    timestep_fns[var_name] = stored;
    return *stored;
}

// [XTS-UnFun] See header for the contract. Index values are always Int literals: object
// elements were already lowered to their global object index by the caller (that is the
// same convention create_object_variable and enumerate_array_domain use), so there is no
// object-sort case to handle here.
z3::expr_vector Z3VariableFactory::cell_args(const std::vector<int64_t>& cell) const {
    z3::expr_vector args(ctx_);
    for (int64_t c : cell) args.push_back(ctx_.int_val(c));
    return args;
}

// [XTS-UnFun] See header for the contract. Arity is derived by counting array
// nesting levels directly from the type (not domain[0].size(), which would force
// callers to enumerate the whole domain just to learn its dimensionality).
//
// The count and the leaf come out of one descend_array_levels() walk (the same one
// leaf_element_type uses), so this no longer traverses the type chain twice — once to
// count, then again inside resolve_elem_sort_at_depth to land on the very same leaf.
// Sets returned above never reach the walk, which is why calling resolve_elem_sort_for_type
// directly is safe here: resolve_elem_sort_at_depth's is_set()/depth<=1 guard existed for
// callers that pass a set type, and this one cannot.
std::pair<unsigned, z3::sort> Z3VariableFactory::resolve_uf_shape(const Type* t) const {
    if (!t) return {1u, ctx_.int_sort()}; // defensive fallback
    if (t->is_set()) return {1u, ctx_.bool_sort()};

    auto [arity, leaf] = descend_array_levels(t, problem_);
    return {arity, resolve_elem_sort_for_type(leaf)};
}

} // namespace rantanplan
