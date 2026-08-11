#include "binding_matcher.hpp"
#include "../util/logger.hpp"
#include <algorithm>
#include <cassert>

namespace rantanplan {

BindingMatcher::BindingMatcher(const Problem& problem, const FactIndex& facts,
                               const ObjectFluentIndex* obj_fluents)
    : problem_(problem), facts_(facts), obj_fluents_(obj_fluents) {}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<PartialBinding> BindingMatcher::find_bindings(const Action& action) const {
    // Step 1: Extract precondition atoms (boolean + object fluent),
    // equality constraints, and object fluent equality filters.
    std::vector<PrecondAtom> atoms;
    std::vector<EqualityConstraint> equalities;
    std::vector<ObjectFluentEqualityFilter> obj_filters;
    if (action.has_precondition()) {
        extract_atoms(action.precondition_id(), action, /*negated=*/false,
                      atoms, equalities, obj_filters);
    }

    // Step 2: Order by selectivity (most constraining first).
    order_by_selectivity(atoms);

    // --- Debug: log extracted atoms ---
    for (const auto& a : atoms) {
        std::string fluent_name = "(unknown)";
        if (a.fluent_schema_id >= 0 &&
            static_cast<size_t>(a.fluent_schema_id) < problem_.fluent_count()) {
            fluent_name = problem_.fluent(a.fluent_schema_id).name();
        }
        std::string params;
        for (size_t i = 0; i < a.arg_param_index.size(); ++i) {
            if (i > 0) params += ",";
            if (a.arg_param_index[i] >= 0) params += "p" + std::to_string(a.arg_param_index[i]);
            else params += "c" + std::to_string(a.arg_constant_obj[i]);
        }
        Logger::instance().debug(
            "  atom: " + fluent_name + "(" + params + ")" +
            (a.negated ? " [NEG]" : "") +
            " facts=" + std::to_string(facts_.get_facts(a.fluent_schema_id).size()));
    }
    // --- End debug ---

    // Step 3: (Negated atom filtering removed — under delete-relaxation,
    // negated preconditions are always satisfiable.  Filtering by them causes
    // under-approximation: e.g. (not (docked ?sh ?from)) is pruned if
    // docked(sh,from) is in the initial state, even though undock can make
    // ¬docked reachable.  The solver handles correctness.)

    // Step 4: Join-based extension — start from a single empty binding.
    std::vector<PartialBinding> bindings;
    bindings.push_back({});

    for (const auto& atom : atoms) {
        // Under delete-relaxation negated atoms are always satisfiable — skip.
        if (atom.negated) {
            Logger::instance().debug(
                "  JOIN skip negated atom schema_id=" +
                std::to_string(atom.fluent_schema_id));
            continue;
        }

        size_t before = bindings.size();
        size_t fact_count = (atom.source == AtomSource::OBJECT_FLUENT && obj_fluents_)
            ? obj_fluents_->get_extended_tuples(atom.fluent_schema_id).size()
            : facts_.get_facts(atom.fluent_schema_id).size();
        bindings = extend_bindings(bindings, atom, action);

        std::string fname = "(?)";
        if (atom.fluent_schema_id >= 0 &&
            static_cast<size_t>(atom.fluent_schema_id) < problem_.fluent_count())
            fname = problem_.fluent(atom.fluent_schema_id).name();

        Logger::instance().debug(
            "  JOIN [" + fname + "] schema_id=" +
            std::to_string(atom.fluent_schema_id) +
            " facts=" + std::to_string(fact_count) +
            " bindings: " + std::to_string(before) +
            " -> " + std::to_string(bindings.size()));

        if (bindings.empty()) break;  // no valid bindings — short-circuit
    }

    // Step 5: Fill parameters not bound by any boolean atom.
    // IMPORTANT: Only fill if the join didn't already eliminate all bindings.
    // If bindings is empty and we had positive atoms, the action is unreachable
    // — don't resurrect it via Cartesian product.
    bool had_positive_atoms = std::any_of(atoms.begin(), atoms.end(),
        [](const PrecondAtom& a) { return !a.negated; });
    if (bindings.empty() && had_positive_atoms) {
        return {};  // join eliminated everything — action unreachable
    }
    bindings = fill_unbound_parameters(bindings, action);

    // --- Temporary debug logging ---
    {
        size_t pos_count = 0, neg_count = 0;
        for (const auto& a : atoms) { if (a.negated) ++neg_count; else ++pos_count; }

        // Count unbound params: those not appearing in any positive atom
        std::vector<bool> bound_by_atom(action.parameter_count(), false);
        for (const auto& a : atoms) {
            if (a.negated) continue;
            for (int pidx : a.arg_param_index) {
                if (pidx >= 0) bound_by_atom[pidx] = true;
            }
        }
        size_t unbound = 0;
        for (bool b : bound_by_atom) { if (!b) ++unbound; }

        Logger::instance().debug(
            "BindingMatcher [" + action.name() + "]: " +
            std::to_string(pos_count) + " pos atoms, " +
            std::to_string(neg_count) + " neg atoms, " +
            std::to_string(unbound) + " unbound params, " +
            std::to_string(bindings.size()) + " bindings after fill");
    }
    // --- End debug logging ---

    // Step 6: Filter by equality constraints (= ?x ?y) and (not (= ?x ?y)).
    if (!equalities.empty()) {
        bindings = filter_by_equality(bindings, equalities);
    }

    // Step 7: Filter by Pattern C object fluent equalities
    // (= (fluent1 ?x) (fluent2 ?y)) — check value set intersection.
    if (!obj_filters.empty() && obj_fluents_) {
        bindings = filter_by_object_fluent_equality(bindings, obj_filters, action);
    }

    // (Negated atom filtering removed — see Step 3 comment above.)

    return bindings;
}

// ---------------------------------------------------------------------------
// Atom extraction
// ---------------------------------------------------------------------------

void BindingMatcher::extract_atoms(ExprID expr_id,
                                    const Action& action,
                                    bool negated,
                                    std::vector<PrecondAtom>& out_atoms,
                                    std::vector<EqualityConstraint>& out_equalities,
                                    std::vector<ObjectFluentEqualityFilter>& out_obj_filters) const {
    if (!expr_id.valid()) return;
    const auto& pool = problem_.pool();

    // Boolean literal constants — nothing to extract.
    if (pool.is_true_constant(expr_id) || pool.is_false_constant(expr_id)) return;

    // ---- Logical connectives ----

    // AND: recurse into all arguments (conjunction — all must hold).
    // Note: FUNCTION_APPLICATION nodes have children[0] = head symbol,
    // arguments start at children[1]. Use arguments() to skip the head.
    if (pool.is_and(expr_id)) {
        if (pool.has_head_and_arguments(expr_id)) {
            for (ExprID arg : pool.arguments(expr_id)) {
                extract_atoms(arg, action, negated, out_atoms, out_equalities, out_obj_filters);
            }
        } else {
            for (ExprID child : pool.children(expr_id)) {
                extract_atoms(child, action, negated, out_atoms, out_equalities, out_obj_filters);
            }
        }
        return;
    }

    // NOT: toggle negation polarity and recurse.
    if (pool.is_not(expr_id)) {
        if (pool.has_head_and_arguments(expr_id)) {
            assert(pool.argument_count(expr_id) == 1);
            extract_atoms(pool.argument(expr_id, 0), action, !negated,
                          out_atoms, out_equalities, out_obj_filters);
        } else {
            assert(pool.child_count(expr_id) == 1);
            extract_atoms(pool.child(expr_id, 0), action, !negated,
                          out_atoms, out_equalities, out_obj_filters);
        }
        return;
    }

    // OR / IMPLIES / IFF: cannot extract mandatory atoms from disjunctive
    // contexts (either branch might suffice).  Skip — over-approximation.
    if (pool.is_or(expr_id) || pool.is_implies(expr_id) || pool.is_iff(expr_id)) {
        return;
    }

    // ---- Equality constraints ----
    // (= ?x ?y) with negated=false  ⇒ must be equal
    // (= ?x ?y) with negated=true   ⇒ must differ  (came through NOT)
    if (pool.is_equals(expr_id)) {
        if (pool.has_head_and_arguments(expr_id) && pool.argument_count(expr_id) == 2) {
            ExprID lhs = pool.argument(expr_id, 0);
            ExprID rhs = pool.argument(expr_id, 1);

            // Resolve a simple argument (parameter or constant) to param/const indices.
            auto resolve_arg = [&](ExprID arg, int& param_out, int& const_out) -> bool {
                param_out = -1;
                const_out = -1;
                if (pool.is_parameter(arg)) {
                    const std::string& pname = pool.payload_string(arg);
                    for (size_t p = 0; p < action.parameter_count(); ++p) {
                        if (action.parameter(p).name() == pname) {
                            param_out = static_cast<int>(p);
                            return true;
                        }
                    }
                    return false; // unknown parameter
                } else if (pool.is_constant(arg) && pool.payload_is_string(arg)) {
                    const std::string& obj_name = pool.payload_string(arg);
                    const Object* obj = problem_.find_object(obj_name);
                    if (obj) {
                        const_out = static_cast<int>(obj - &problem_.objects()[0]);
                        return true;
                    }
                    return false;
                }
                return false; // complex expression — can't handle
            };

            // Decompose an object fluent STATE_VARIABLE into schema_id + per-arg
            // param/constant indices. Returns false if not an object fluent.
            auto decompose_fluent_arg = [&](ExprID arg,
                                             int& schema_out,
                                             std::vector<int>& arg_params,
                                             std::vector<int>& arg_consts) -> bool {
                if (!pool.is_state_variable(arg)) return false;

                ExprID head = pool.head_symbol_id(arg);
                if (!pool.is_fluent_symbol(head)) return false;

                const std::string& fname = pool.payload_string(head);
                const Fluent* fluent = problem_.find_fluent(fname);
                if (!fluent || !fluent->is_object_fluent()) return false;

                schema_out = fluent->id();
                size_t nargs = pool.argument_count(arg);
                arg_params.resize(nargs, -1);
                arg_consts.resize(nargs, -1);

                for (size_t i = 0; i < nargs; ++i) {
                    ExprID a = pool.argument(arg, i);
                    if (pool.is_parameter(a)) {
                        const std::string& pname = pool.payload_string(a);
                        bool found = false;
                        for (size_t p = 0; p < action.parameter_count(); ++p) {
                            if (action.parameter(p).name() == pname) {
                                arg_params[i] = static_cast<int>(p);
                                found = true;
                                break;
                            }
                        }
                        if (!found) return false;
                    } else if (pool.is_constant(a) && pool.payload_is_string(a)) {
                        const Object* obj = problem_.find_object(pool.payload_string(a));
                        if (!obj) return false;
                        arg_consts[i] = static_cast<int>(obj - &problem_.objects()[0]);
                    } else {
                        return false;  // Nested expression in fluent arg.
                    }
                }
                return true;
            };

            // Try simple param/constant resolution first (existing behavior).
            EqualityConstraint eq;
            bool lhs_ok = resolve_arg(lhs, eq.param_a, eq.const_a);
            bool rhs_ok = resolve_arg(rhs, eq.param_b, eq.const_b);

            if (lhs_ok && rhs_ok) {
                eq.negated = negated;
                out_equalities.push_back(eq);
            } else if (obj_fluents_) {
                // Try object fluent decomposition.
                int lhs_schema = -1, rhs_schema = -1;
                std::vector<int> lhs_aparams, lhs_aconsts, rhs_aparams, rhs_aconsts;
                bool lhs_fluent = decompose_fluent_arg(lhs, lhs_schema, lhs_aparams, lhs_aconsts);
                bool rhs_fluent = decompose_fluent_arg(rhs, rhs_schema, rhs_aparams, rhs_aconsts);

                if (lhs_fluent && !rhs_fluent && rhs_ok) {
                    // Pattern A/B: (= (fluent ?x...) ?y) or (= (fluent ?x...) const)
                    // Create PrecondAtom with source=OBJECT_FLUENT.
                    // Extended tuple columns = [fluent_args..., value].
                    PrecondAtom atom;
                    atom.fluent_schema_id = lhs_schema;
                    atom.arg_param_index  = std::move(lhs_aparams);
                    atom.arg_constant_obj = std::move(lhs_aconsts);
                    // Append value column from RHS.
                    atom.arg_param_index.push_back(eq.param_b);
                    atom.arg_constant_obj.push_back(eq.const_b);
                    atom.negated = negated;
                    atom.source  = AtomSource::OBJECT_FLUENT;
                    out_atoms.push_back(std::move(atom));
                } else if (!lhs_fluent && lhs_ok && rhs_fluent) {
                    // Pattern A/B: (= ?y (fluent ?x...)) — swapped sides.
                    PrecondAtom atom;
                    atom.fluent_schema_id = rhs_schema;
                    atom.arg_param_index  = std::move(rhs_aparams);
                    atom.arg_constant_obj = std::move(rhs_aconsts);
                    // Append value column from LHS.
                    atom.arg_param_index.push_back(eq.param_a);
                    atom.arg_constant_obj.push_back(eq.const_a);
                    atom.negated = negated;
                    atom.source  = AtomSource::OBJECT_FLUENT;
                    out_atoms.push_back(std::move(atom));
                } else if (lhs_fluent && rhs_fluent) {
                    // Pattern C: (= (fluent1 ?x...) (fluent2 ?y...))
                    // Handle as post-filter.
                    ObjectFluentEqualityFilter filter;
                    filter.lhs_schema_id       = lhs_schema;
                    filter.lhs_arg_param_index = std::move(lhs_aparams);
                    filter.lhs_arg_constant_obj = std::move(lhs_aconsts);
                    filter.rhs_schema_id       = rhs_schema;
                    filter.rhs_arg_param_index = std::move(rhs_aparams);
                    filter.rhs_arg_constant_obj = std::move(rhs_aconsts);
                    filter.negated = negated;
                    out_obj_filters.push_back(std::move(filter));
                }
                // else: neither side is an object fluent — skip (numeric, etc.)
            }
        }
        return;
    }

    // ---- Numeric / comparison sub-trees ----

    // All comparisons are assumed satisfiable under delete-relaxation.
    if (pool.is_less_than(expr_id) || pool.is_less_equal(expr_id) ||
        pool.is_greater_than(expr_id) || pool.is_greater_equal(expr_id)) {
        return;
    }

    // ---- Boolean fluent application ----

    if (pool.is_state_variable(expr_id)) {
        ExprID head = pool.head_symbol_id(expr_id);
        if (!pool.is_fluent_symbol(head)) return;

        const std::string& fluent_name = pool.payload_string(head);
        const Fluent* fluent = problem_.find_fluent(fluent_name);
        if (!fluent || !fluent->is_predicate()) return; // skip non-boolean

        PrecondAtom atom;
        atom.fluent_schema_id = fluent->id();
        atom.negated           = negated;

        const size_t num_args = pool.argument_count(expr_id);
        atom.arg_param_index.resize(num_args, -1);
        atom.arg_constant_obj.resize(num_args, -1);

        bool usable = true;
        for (size_t i = 0; i < num_args && usable; ++i) {
            ExprID arg = pool.argument(expr_id, i);

            if (pool.is_parameter(arg)) {
                const std::string& pname = pool.payload_string(arg);
                // Linear scan — actions typically have ≤5 parameters.
                bool found = false;
                for (size_t p = 0; p < action.parameter_count(); ++p) {
                    if (action.parameter(p).name() == pname) {
                        atom.arg_param_index[i] = static_cast<int>(p);
                        found = true;
                        break;
                    }
                }
                if (!found) usable = false;
            } else if (pool.is_constant(arg) && pool.payload_is_string(arg)) {
                const std::string& obj_name = pool.payload_string(arg);
                const Object* obj = problem_.find_object(obj_name);
                if (obj) {
                    atom.arg_constant_obj[i] =
                        static_cast<int>(obj - &problem_.objects()[0]);
                } else {
                    usable = false;
                }
            } else {
                // Complex or nested expression argument — skip atom.
                usable = false;
            }
        }

        if (usable) {
            out_atoms.push_back(std::move(atom));
        }
        return;
    }

    // FUNCTION_APPLICATION or any other kind — skip.
}

// ---------------------------------------------------------------------------
// Selectivity ordering
// ---------------------------------------------------------------------------

void BindingMatcher::order_by_selectivity(std::vector<PrecondAtom>& atoms) const {
    // Score = |matching facts| / max(1, #parameterised args in atom)
    // Lower score → more selective → process first.
    // Negated atoms always sort to the end (they are skipped anyway).

    std::sort(atoms.begin(), atoms.end(),
        [this](const PrecondAtom& a, const PrecondAtom& b) {
            if (a.negated != b.negated) return !a.negated; // positive first

            int a_params = 0, b_params = 0;
            for (int idx : a.arg_param_index) { if (idx >= 0) ++a_params; }
            for (int idx : b.arg_param_index) { if (idx >= 0) ++b_params; }

            auto get_fact_count = [this](const PrecondAtom& atom) -> double {
                if (atom.source == AtomSource::OBJECT_FLUENT && obj_fluents_) {
                    return static_cast<double>(
                        obj_fluents_->get_extended_tuples(atom.fluent_schema_id).size());
                }
                return static_cast<double>(
                    facts_.get_facts(atom.fluent_schema_id).size());
            };

            double a_score = get_fact_count(a) / std::max(1, a_params);
            double b_score = get_fact_count(b) / std::max(1, b_params);

            return a_score < b_score;
        });
}

// ---------------------------------------------------------------------------
// Join extension
// ---------------------------------------------------------------------------

std::vector<PartialBinding> BindingMatcher::extend_bindings(
    const std::vector<PartialBinding>& current_bindings,
    const PrecondAtom& atom,
    const Action& action) const
{
    std::vector<PartialBinding> result;

    const auto& tuples = (atom.source == AtomSource::OBJECT_FLUENT && obj_fluents_)
        ? obj_fluents_->get_extended_tuples(atom.fluent_schema_id)
        : facts_.get_facts(atom.fluent_schema_id);

    for (const auto& binding : current_bindings) {
        for (const auto& tuple : tuples) {
            bool consistent = true;
            PartialBinding extended = binding;

            for (size_t i = 0; i < atom.arg_param_index.size() && consistent; ++i) {
                const int param_idx    = atom.arg_param_index[i];
                const int constant_idx = atom.arg_constant_obj[i];
                const int tuple_val    = tuple[i];

                if (constant_idx >= 0) {
                    // Argument is a fixed constant — tuple must match.
                    if (tuple_val != constant_idx) consistent = false;
                } else if (param_idx >= 0) {
                    auto it = extended.find(param_idx);
                    if (it != extended.end()) {
                        // Already bound — must agree.
                        if (it->second != tuple_val) consistent = false;
                    } else {
                        // New binding — verify type compatibility.
                        if (is_type_compatible(action, param_idx, tuple_val)) {
                            extended[param_idx] = tuple_val;
                        } else {
                            consistent = false;
                        }
                    }
                }
            }

            if (consistent) {
                result.push_back(std::move(extended));
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Fill unbound parameters
// ---------------------------------------------------------------------------

std::vector<PartialBinding> BindingMatcher::fill_unbound_parameters(
    const std::vector<PartialBinding>& bindings,
    const Action& action) const
{
    if (action.parameter_count() == 0) return bindings;

    // Detect which parameters are unbound.
    // All bindings have the same bound-param-set (same join path), so check first.
    std::vector<int> unbound;
    if (bindings.empty()) {
        // No bindings at all — enumerate everything.
        for (size_t i = 0; i < action.parameter_count(); ++i)
            unbound.push_back(static_cast<int>(i));
    } else {
        const auto& first = bindings.front();
        for (size_t i = 0; i < action.parameter_count(); ++i) {
            if (first.find(static_cast<int>(i)) == first.end())
                unbound.push_back(static_cast<int>(i));
        }
    }

    if (unbound.empty()) return bindings;

    // Pre-compute type-compatible object lists (once).
    auto type_compat = get_type_compatible_objects(action);

    // Cartesian-product expansion of unbound parameters.
    const auto& start = bindings.empty()
        ? std::vector<PartialBinding>{PartialBinding{}}
        : bindings;

    std::vector<PartialBinding> result = start;

    for (int pidx : unbound) {
        const auto& compatible = type_compat[pidx];
        std::vector<PartialBinding> expanded;
        expanded.reserve(result.size() * compatible.size());

        for (const auto& binding : result) {
            for (int oidx : compatible) {
                PartialBinding ext = binding;
                ext[pidx] = oidx;
                expanded.push_back(std::move(ext));
            }
        }

        result = std::move(expanded);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Type compatibility helpers
// ---------------------------------------------------------------------------

bool BindingMatcher::is_type_compatible(const Action& action,
                                         int param_idx, int obj_idx) const {
    const Type* ptype = action.parameter(param_idx).type();

    // For bounded-int parameters, check range
    const Type* bounded = ptype ? ptype->bounded_int_ancestor() : nullptr;
    if (bounded) {
        return obj_idx >= static_cast<int>(bounded->lower_bound()) &&
               obj_idx <= static_cast<int>(bounded->upper_bound());
    }

    const Type* otype = problem_.object(obj_idx).type();

    if (!ptype || !otype) return true;  // untyped ⇒ always compatible

    return otype->is_subtype_of(ptype);
}

std::vector<std::vector<int>> BindingMatcher::get_type_compatible_objects(
    const Action& action) const
{
    std::vector<std::vector<int>> compat(action.parameter_count());

    for (size_t p = 0; p < action.parameter_count(); ++p) {
        const Type* ptype = action.parameter(p).type();
        const Type* bounded = ptype ? ptype->bounded_int_ancestor() : nullptr;

        if (bounded) {
            // Enumerate integer values [lo, hi] for bounded-int parameters.
            for (int64_t v = bounded->lower_bound(); v <= bounded->upper_bound(); ++v) {
                compat[p].push_back(static_cast<int>(v));
            }
        } else {
            // Enumerate type-compatible objects (existing behaviour).
            for (size_t o = 0; o < problem_.object_count(); ++o) {
                if (is_type_compatible(action, static_cast<int>(p), static_cast<int>(o))) {
                    compat[p].push_back(static_cast<int>(o));
                }
            }
        }
    }

    return compat;
}

// ---------------------------------------------------------------------------
// Equality constraint filtering
// ---------------------------------------------------------------------------

std::vector<PartialBinding> BindingMatcher::filter_by_equality(
    const std::vector<PartialBinding>& bindings,
    const std::vector<EqualityConstraint>& equalities) const
{
    std::vector<PartialBinding> result;
    result.reserve(bindings.size());

    for (const auto& binding : bindings) {
        bool pass = true;
        for (const auto& eq : equalities) {
            // Resolve left-hand side to an object index.
            int lhs_obj = -1;
            if (eq.param_a >= 0) {
                auto it = binding.find(eq.param_a);
                if (it != binding.end()) lhs_obj = it->second;
                else continue; // unbound — can't check, skip constraint
            } else {
                lhs_obj = eq.const_a;
            }

            // Resolve right-hand side to an object index.
            int rhs_obj = -1;
            if (eq.param_b >= 0) {
                auto it = binding.find(eq.param_b);
                if (it != binding.end()) rhs_obj = it->second;
                else continue; // unbound — can't check, skip constraint
            } else {
                rhs_obj = eq.const_b;
            }

            if (lhs_obj < 0 || rhs_obj < 0) continue;

            if (eq.negated) {
                // (not (= ?x ?y)) ⇒ must differ
                if (lhs_obj == rhs_obj) { pass = false; break; }
            } else {
                // (= ?x ?y) ⇒ must be equal
                if (lhs_obj != rhs_obj) { pass = false; break; }
            }
        }
        if (pass) result.push_back(binding);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Negated atom filtering
// ---------------------------------------------------------------------------

std::vector<PartialBinding> BindingMatcher::filter_by_negated_atoms(
    const std::vector<PartialBinding>& bindings,
    const std::vector<PrecondAtom>& negated_atoms) const
{
    std::vector<PartialBinding> result;
    result.reserve(bindings.size());

    for (const auto& binding : bindings) {
        bool pass = true;
        for (const auto& atom : negated_atoms) {
            // Build the ground tuple for this negated atom under the binding.
            std::vector<int> ground_tuple(atom.arg_param_index.size());
            bool fully_ground = true;

            for (size_t i = 0; i < atom.arg_param_index.size(); ++i) {
                if (atom.arg_constant_obj[i] >= 0) {
                    ground_tuple[i] = atom.arg_constant_obj[i];
                } else if (atom.arg_param_index[i] >= 0) {
                    auto it = binding.find(atom.arg_param_index[i]);
                    if (it != binding.end()) {
                        ground_tuple[i] = it->second;
                    } else {
                        fully_ground = false;
                        break;
                    }
                } else {
                    fully_ground = false;
                    break;
                }
            }

            if (!fully_ground) continue; // can't check — keep binding

            // The atom is negated: precondition says "NOT fluent(args)".
            // If the fact IS present, the precondition is violated ⇒ prune.
            if (facts_.contains(atom.fluent_schema_id, ground_tuple)) {
                pass = false;
                break;
            }
        }
        if (pass) result.push_back(binding);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Object fluent equality filter (Pattern C)
// ---------------------------------------------------------------------------

std::vector<PartialBinding> BindingMatcher::filter_by_object_fluent_equality(
    const std::vector<PartialBinding>& bindings,
    const std::vector<ObjectFluentEqualityFilter>& filters,
    const Action& action) const
{
    if (!obj_fluents_) return bindings;

    std::vector<PartialBinding> result;
    result.reserve(bindings.size());

    for (const auto& binding : bindings) {
        bool pass = true;
        for (const auto& filter : filters) {
            // Under delete-relaxation, negated conditions are always satisfiable
            // (value sets only grow, so disequality can't be guaranteed). Skip.
            if (filter.negated) continue;

            // Resolve LHS fluent arg tuple.
            auto resolve_args = [&](const std::vector<int>& param_idx,
                                     const std::vector<int>& const_obj) -> std::vector<int> {
                std::vector<int> args(param_idx.size());
                for (size_t i = 0; i < param_idx.size(); ++i) {
                    if (const_obj[i] >= 0) {
                        args[i] = const_obj[i];
                    } else if (param_idx[i] >= 0) {
                        auto it = binding.find(param_idx[i]);
                        if (it != binding.end()) {
                            args[i] = it->second;
                        } else {
                            return {};  // Unbound parameter — can't check.
                        }
                    } else {
                        return {};
                    }
                }
                return args;
            };

            auto lhs_args = resolve_args(filter.lhs_arg_param_index,
                                          filter.lhs_arg_constant_obj);
            auto rhs_args = resolve_args(filter.rhs_arg_param_index,
                                          filter.rhs_arg_constant_obj);

            // If we couldn't fully resolve args, skip the filter (safe).
            if (lhs_args.empty() || rhs_args.empty()) continue;

            const auto& lhs_values = obj_fluents_->get_values(
                filter.lhs_schema_id, lhs_args);
            const auto& rhs_values = obj_fluents_->get_values(
                filter.rhs_schema_id, rhs_args);

            // Both fluent instances must have at least one reachable value,
            // and their value sets must intersect for equality to be satisfiable.
            if (lhs_values.empty() || rhs_values.empty()) {
                pass = false;
                break;
            }

            // Check intersection: iterate the smaller set, probe the larger.
            const auto& smaller = (lhs_values.size() <= rhs_values.size())
                ? lhs_values : rhs_values;
            const auto& larger  = (lhs_values.size() <= rhs_values.size())
                ? rhs_values : lhs_values;

            bool intersects = false;
            for (int v : smaller) {
                if (larger.count(v)) {
                    intersects = true;
                    break;
                }
            }

            if (!intersects) {
                pass = false;
                break;
            }
        }
        if (pass) result.push_back(binding);
    }

    return result;
}

} // namespace rantanplan
