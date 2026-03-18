#include "reachability_grounder.hpp"
#include "binding_matcher.hpp"
#include "action_instantiator.hpp"
#include "../config/config.hpp"
#include "../util/logger.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/stats.hpp"
#include <unordered_set>
#include <cassert>

namespace rantanplan {

// ---------------------------------------------------------------------------
// Binding fingerprint: compact representation for deduplication.
// ---------------------------------------------------------------------------

/// Convert a PartialBinding to a canonical vector (object index per param,
/// ordered by param index).  Used as deduplication key.
static std::vector<int> binding_fingerprint(const PartialBinding& binding,
                                             size_t num_params) {
    std::vector<int> fp(num_params, -1);
    for (const auto& [pidx, oidx] : binding) {
        fp[pidx] = oidx;
    }
    return fp;
}

/// Hash a fingerprint vector (FNV-1a).  Used as the hasher for the
/// deduplication set below, which stores full fingerprints and resolves
/// collisions via equality (previously only the 64-bit hash was stored,
/// meaning a collision would silently skip a distinct binding).
struct FingerprintHash {
    size_t operator()(const std::vector<int>& fp) const {
        uint64_t h = 14695981039346656037ULL;
        for (int v : fp) {
            h ^= static_cast<uint64_t>(static_cast<uint32_t>(v));
            h *= 1099511628211ULL;
        }
        return static_cast<size_t>(h);
    }
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ReachabilityGrounder::ReachabilityGrounder(const Problem& lifted_problem)
    : lifted_problem_(lifted_problem) {}

// ---------------------------------------------------------------------------
// collect_add_effects
// ---------------------------------------------------------------------------

size_t ReachabilityGrounder::collect_add_effects(const Action& ground_action,
                                                  FactIndex& facts) const {
    size_t new_facts = 0;
    const auto& pool = lifted_problem_.pool();

    for (const auto& eff : ground_action.effects()) {
        const auto& ee = eff.effect_expression();

        // Only ASSIGN effects can set boolean fluents to true.
        // INCREASE/DECREASE only apply to numeric fluents.
        if (!ee.is_assign()) continue;

        ExprID fluent_eid = ee.fluent_id();
        ExprID value_eid  = ee.value_id();

        // We're in a delete-relaxation: only ADD effects matter (value = true).
        if (!pool.is_true_constant(value_eid)) continue;

        // Decompose the ground fluent application to (schema_id, object_indices).
        if (!pool.is_state_variable(fluent_eid)) continue;

        ExprID head = pool.head_symbol_id(fluent_eid);
        if (!pool.is_fluent_symbol(head)) continue;

        const std::string& fname = pool.payload_string(head);
        const Fluent* fluent_schema = lifted_problem_.find_fluent(fname);
        if (!fluent_schema || !fluent_schema->is_predicate()) continue;

        // Extract object indices from the ground fluent arguments.
        size_t nargs = pool.argument_count(fluent_eid);
        std::vector<int> obj_indices;
        obj_indices.reserve(nargs);

        bool valid = true;
        for (size_t i = 0; i < nargs && valid; ++i) {
            ExprID arg = pool.argument(fluent_eid, i);
            if (pool.is_constant(arg) && pool.payload_is_string(arg)) {
                const Object* obj = lifted_problem_.find_object(pool.payload_string(arg));
                if (obj) {
                    obj_indices.push_back(
                        static_cast<int>(obj - &lifted_problem_.objects()[0]));
                } else {
                    valid = false;
                }
            } else {
                valid = false;
            }
        }

        if (valid && facts.add_fact(fluent_schema->id(), obj_indices)) {
            ++new_facts;
        }
    }

    return new_facts;
}

// ---------------------------------------------------------------------------
// collect_object_fluent_effects
// ---------------------------------------------------------------------------

size_t ReachabilityGrounder::collect_object_fluent_effects(
    const Action& ground_action, ObjectFluentIndex& obj_fluents) const
{
    size_t new_values = 0;
    const auto& pool = lifted_problem_.pool();

    for (const auto& eff : ground_action.effects()) {
        const auto& ee = eff.effect_expression();

        // Only ASSIGN effects are relevant for object fluents.
        if (!ee.is_assign()) continue;

        ExprID fluent_eid = ee.fluent_id();
        ExprID value_eid  = ee.value_id();

        // Decompose fluent — only object fluents.
        if (!pool.is_state_variable(fluent_eid)) continue;

        ExprID head = pool.head_symbol_id(fluent_eid);
        if (!pool.is_fluent_symbol(head)) continue;

        const std::string& fname = pool.payload_string(head);
        const Fluent* fluent_schema = lifted_problem_.find_fluent(fname);
        if (!fluent_schema || !fluent_schema->is_object_fluent()) continue;

        // Extract object indices from the ground fluent arguments.
        size_t nargs = pool.argument_count(fluent_eid);
        std::vector<int> arg_indices;
        arg_indices.reserve(nargs);

        bool valid = true;
        for (size_t i = 0; i < nargs && valid; ++i) {
            ExprID arg = pool.argument(fluent_eid, i);
            if (pool.is_constant(arg) && pool.payload_is_string(arg)) {
                const Object* obj = lifted_problem_.find_object(pool.payload_string(arg));
                if (obj) {
                    arg_indices.push_back(
                        static_cast<int>(obj - &lifted_problem_.objects()[0]));
                } else {
                    valid = false;
                }
            } else {
                valid = false;
            }
        }
        if (!valid) continue;

        // Resolve the value to an object index.
        if (!pool.is_constant(value_eid) || !pool.payload_is_string(value_eid)) continue;

        const Object* val_obj = lifted_problem_.find_object(pool.payload_string(value_eid));
        if (!val_obj) continue;

        int val_idx = static_cast<int>(val_obj - &lifted_problem_.objects()[0]);

        if (obj_fluents.add_value(fluent_schema->id(), arg_indices, val_idx)) {
            ++new_values;
        }
    }

    return new_values;
}

// ---------------------------------------------------------------------------
// collect_numeric_effects
// ---------------------------------------------------------------------------

bool ReachabilityGrounder::collect_numeric_effects(const Action& ground_action,
                                                    NumericBoundsIndex& bounds) const {
    bool any_changed = false;
    const auto& pool = lifted_problem_.pool();

    for (const auto& eff : ground_action.effects()) {
        const auto& ee = eff.effect_expression();

        // For conditional effects, assume the condition can hold (delete-relaxation
        // over-approximation).  Skipping them would under-approximate the reachable
        // interval — e.g. missing a fuel decrease keeps the lower bound too high,
        // causing unsound pruning of refuel actions.

        ExprID fluent_eid = ee.fluent_id();

        // Decompose the fluent — only numeric fluents.
        int schema_id = -1;
        std::vector<int> obj_indices;
        if (!bounds.decompose_numeric_state_variable(fluent_eid, schema_id, obj_indices)) {
            continue;  // Boolean fluent or decomposition failed.
        }

        Interval old_bounds = bounds.get_bounds(schema_id, obj_indices);

        Interval new_val(0.0);  // Will be overwritten.

        if (ee.is_assign()) {
            // ASSIGN X = Y: new reachable interval is eval(Y).
            new_val = evaluate_interval(ee.value_id(), pool, bounds, lifted_problem_);
        } else if (ee.is_increase()) {
            // INCREASE X by Y: new reachable interval is [old.min + Y.min, old.max + Y.max].
            Interval delta = evaluate_interval(ee.value_id(), pool, bounds, lifted_problem_);
            new_val = old_bounds + delta;
        } else if (ee.is_decrease()) {
            // DECREASE X by Y: new reachable interval is [old.min - Y.max, old.max - Y.min].
            Interval delta = evaluate_interval(ee.value_id(), pool, bounds, lifted_problem_);
            new_val = old_bounds - delta;
        } else {
            continue;  // Unknown effect kind.
        }

        // Under delete-relaxation, update via convex union (old values still reachable).
        if (bounds.update_bounds(schema_id, obj_indices, new_val)) {
            any_changed = true;
        }

        // Apply widening to guarantee termination.
        bounds.maybe_widen(schema_id, obj_indices);
    }

    return any_changed;
}

// ---------------------------------------------------------------------------
// goals_reachable
// ---------------------------------------------------------------------------

bool ReachabilityGrounder::goals_reachable(const FactIndex& facts,
                                            const NumericBoundsIndex* numeric_bounds) const {
    const auto& pool = lifted_problem_.pool();

    for (const auto& goal : lifted_problem_.goals()) {
        ExprID gid = goal.goal_id();
        if (!gid.valid()) continue;

        // Only check boolean fluent atoms at top level.
        // Walk through ANDs.
        // For complex goals (OR, numeric), we can't prove unreachability easily,
        // so we skip them (assume satisfiable) — unless numeric bounds are available.
        std::vector<ExprID> stack = {gid};
        while (!stack.empty()) {
            ExprID eid = stack.back();
            stack.pop_back();

            if (pool.is_and(eid)) {
                if (pool.has_head_and_arguments(eid)) {
                    for (ExprID arg : pool.arguments(eid)) {
                        stack.push_back(arg);
                    }
                } else {
                    for (ExprID child : pool.children(eid)) {
                        stack.push_back(child);
                    }
                }
                continue;
            }

            // A positive boolean fluent atom in the goal.
            if (pool.is_state_variable(eid)) {
                ExprID head = pool.head_symbol_id(eid);
                if (!pool.is_fluent_symbol(head)) continue;

                const std::string& fname = pool.payload_string(head);
                const Fluent* fluent_schema = lifted_problem_.find_fluent(fname);
                if (!fluent_schema || !fluent_schema->is_predicate()) continue;

                size_t nargs = pool.argument_count(eid);
                std::vector<int> obj_indices;
                obj_indices.reserve(nargs);

                bool valid = true;
                for (size_t i = 0; i < nargs && valid; ++i) {
                    ExprID arg = pool.argument(eid, i);
                    if (pool.is_constant(arg) && pool.payload_is_string(arg)) {
                        const Object* obj = lifted_problem_.find_object(
                            pool.payload_string(arg));
                        if (obj) {
                            obj_indices.push_back(
                                static_cast<int>(obj - &lifted_problem_.objects()[0]));
                        } else {
                            valid = false;
                        }
                    } else {
                        valid = false;
                    }
                }

                if (valid && !facts.contains(fluent_schema->id(), obj_indices)) {
                    return false;  // This goal atom is not reachable.
                }
                continue;
            }

            // Numeric comparison in goal — check with interval bounds if available.
            if (numeric_bounds && pool.is_function_application(eid)) {
                ExprOperator op = pool.op(eid);
                if (op == ExprOperator::LESS_EQUAL  || op == ExprOperator::LESS_THAN ||
                    op == ExprOperator::GREATER_EQUAL || op == ExprOperator::GREATER_THAN ||
                    op == ExprOperator::EQUALS) {
                    if (!numeric_precondition_satisfiable(eid, pool, *numeric_bounds, lifted_problem_)) {
                        return false;  // Numeric goal provably unreachable.
                    }
                    continue;
                }
            }

            // NOT, OR, IMPLIES, IFF, etc. — skip (assume satisfiable).
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Main grounding loop
// ---------------------------------------------------------------------------

GroundingResult ReachabilityGrounder::ground() {
    ScopedTimer timer("grounding_time");

    ExprPool& pool = *lifted_problem_.pool_ptr();
    const bool use_numeric = Config::instance().global.numeric_grounding;

    // 1. Initialize FactIndex from initial state.
    FactIndex facts(lifted_problem_);
    facts.initialize_from_initial_state();

    // 1a. Initialize ObjectFluentIndex from initial state.
    ObjectFluentIndex obj_fluents(lifted_problem_);
    obj_fluents.initialize_from_initial_state();

    if (obj_fluents.total_entry_count() > 0) {
        Logger::instance().component(VerbosityLevel::INFO, "Grounding", {
            {"initial object fluent values", std::to_string(obj_fluents.total_entry_count())}
        });
    }

    // 1b. Optionally initialize NumericBoundsIndex.
    std::unique_ptr<NumericBoundsIndex> numeric_bounds;
    if (use_numeric) {
        numeric_bounds = std::make_unique<NumericBoundsIndex>(lifted_problem_);
        numeric_bounds->initialize_from_initial_state();
        numeric_bounds->precompute_freezes();
        Logger::instance().component(VerbosityLevel::INFO, "Grounding", {
            {"numeric bounds", "enabled"}
        });
    }

    Logger::instance().component(VerbosityLevel::INFO, "Grounding", {
        {"initial facts", std::to_string(facts.total_fact_count())},
        {"action schemas", std::to_string(lifted_problem_.action_count())}
    });

    // Per-schema seen-binding sets for deduplication (full fingerprint, not hash-only).
    std::vector<std::unordered_set<std::vector<int>, FingerprintHash>> seen_bindings(lifted_problem_.action_count());

    // Accumulate all ground actions across iterations.
    std::vector<Action> ground_actions;

    int iteration = 0;
    int goal_reachable_layer = -1;

    // Track bindings that were numerically pruned — keyed by (schema_idx, fingerprint).
    // Stores the action name for logging.  Entries are removed if the binding is
    // later accepted after bounds widen.  What remains at fixpoint is permanently pruned.
    std::vector<std::unordered_map<std::vector<int>, std::string, FingerprintHash>> numeric_pruned_bindings;
    if (use_numeric) {
        numeric_pruned_bindings.resize(lifted_problem_.action_count());
    }

    // Check goal reachability in the initial state (layer 0).
    if (goals_reachable(facts, numeric_bounds.get())) {
        goal_reachable_layer = 0;
    }

    bool changed = true;

    while (changed) {
        changed = false;
        ++iteration;

        BindingMatcher matcher(lifted_problem_, facts, &obj_fluents);

        size_t new_actions_this_iter = 0;
        size_t new_facts_this_iter   = 0;
        size_t pruned_this_iter      = 0;

        for (size_t schema_idx = 0; schema_idx < lifted_problem_.action_count(); ++schema_idx) {
            const Action& schema = lifted_problem_.action(schema_idx);

            auto bindings = matcher.find_bindings(schema);

            for (auto& binding : bindings) {
                auto fp = binding_fingerprint(binding, schema.parameter_count());

                if (seen_bindings[schema_idx].count(fp)) {
                    continue;  // Already instantiated this binding.
                }

                // Instantiate the ground action.
                Action ga = instantiate_action(pool, lifted_problem_, schema, binding);

                // Numeric precondition check: prune if provably unsatisfiable.
                // Do NOT add to seen_bindings yet — bounds may widen in a later
                // iteration (e.g. after refuel increases fuel), so we must retry.
                if (use_numeric && ga.has_precondition() &&
                    !numeric_precondition_satisfiable(ga.precondition_id(), pool,
                                                      *numeric_bounds, lifted_problem_)) {
                    ++pruned_this_iter;
                    // Record for permanent-prune tracking (only first time).
                    if (!numeric_pruned_bindings[schema_idx].count(fp)) {
                        std::string desc = ga.name() + "(";
                        const auto& objects = lifted_problem_.objects();
                        for (size_t pi = 0; pi < fp.size(); ++pi) {
                            if (pi > 0) desc += ", ";
                            desc += (fp[pi] >= 0 && fp[pi] < static_cast<int>(objects.size()))
                                    ? objects[fp[pi]].name() : "?";
                        }
                        desc += ")";
                        numeric_pruned_bindings[schema_idx][fp] = desc;
                    }
                    continue;
                }

                // Action passed all checks — mark as seen so we don't retry it.
                seen_bindings[schema_idx].insert(fp);

                // If this binding was previously pruned, it's no longer permanently pruned.
                if (use_numeric) {
                    numeric_pruned_bindings[schema_idx].erase(fp);
                }

                size_t nf = collect_add_effects(ga, facts);
                if (nf > 0) {
                    new_facts_this_iter += nf;
                    changed = true;
                }

                // Collect object fluent effects.
                size_t new_obj = collect_object_fluent_effects(ga, obj_fluents);
                if (new_obj > 0) {
                    new_facts_this_iter += new_obj;
                    changed = true;
                }

                // Collect numeric effects and update bounds.
                if (use_numeric) {
                    collect_numeric_effects(ga, *numeric_bounds);
                }

                ground_actions.push_back(std::move(ga));
                ++new_actions_this_iter;
            }
        }

        // Re-apply numeric effects of all accepted actions until bounds stabilize.
        // Numeric effects are NOT idempotent: e.g. increment(c) adds rate_value(c)
        // to value(c), and if rate_value widened since the action was first processed,
        // value must widen too.  We iterate until no further change.
        if (use_numeric) {
            bool any_numeric_widened = false;
            bool numeric_changed = true;
            while (numeric_changed) {
                numeric_changed = false;
                for (const auto& ga : ground_actions) {
                    if (collect_numeric_effects(ga, *numeric_bounds)) {
                        numeric_changed = true;
                        any_numeric_widened = true;
                    }
                }
            }
            // If there are still numerically-pruned bindings outstanding and
            // bounds may have changed (either during initial effect collection
            // from new actions, or during the inner fixpoint), re-check them.
            if (new_actions_this_iter > 0 || any_numeric_widened) {
                bool has_pruned = false;
                for (const auto& m : numeric_pruned_bindings) {
                    if (!m.empty()) { has_pruned = true; break; }
                }
                if (has_pruned) {
                    changed = true;
                }
            }
        }

        // Check goal reachability at this layer.
        if (goal_reachable_layer < 0 && goals_reachable(facts, numeric_bounds.get())) {
            goal_reachable_layer = iteration;
        }

        std::string iter_name = "Grounding I" + std::to_string(iteration);
        std::vector<std::pair<std::string, std::string>> metrics = {
            {"new actions", std::to_string(new_actions_this_iter)},
            {"new facts", std::to_string(new_facts_this_iter)},
            {"total facts", std::to_string(facts.total_fact_count())}
        };
        if (use_numeric && pruned_this_iter > 0) {
            metrics.push_back({"numeric pruned", std::to_string(pruned_this_iter)});
        }
        Logger::instance().component(VerbosityLevel::INFO, iter_name, metrics);
    }

    // Build result.
    GroundingResult result;
    result.iterations          = iteration;
    result.ground_action_count = ground_actions.size();
    result.lower_bound         = std::max(0, goal_reachable_layer);
    result.proven_unsolvable   = (goal_reachable_layer < 0);

    Logger::instance().component(VerbosityLevel::INFO, "Grounding", {
        {"fixpoint", std::to_string(iteration) + " iterations"},
        {"actions", std::to_string(ground_actions.size())},
        {"reachable facts", std::to_string(facts.total_fact_count())}
    });

    if (use_numeric) {
        size_t permanently_pruned = 0;
        for (size_t si = 0; si < numeric_pruned_bindings.size(); ++si) {
            for (const auto& [fp, desc] : numeric_pruned_bindings[si]) {
                Logger::instance().debug("Grounding: permanently pruned by numeric bounds — " + desc);
                ++permanently_pruned;
            }
        }
        if (permanently_pruned > 0) {
            Logger::instance().component(VerbosityLevel::INFO, "Grounding", {
                {"numeric pruned", std::to_string(permanently_pruned)}
            });
            Stats::instance().set("grounding_numeric_pruned", static_cast<int>(permanently_pruned));
        }
    }

    if (result.proven_unsolvable) {
        Logger::instance().component(VerbosityLevel::INFO, "Grounding", {
            {"result", "UNSOLVABLE (goals unreachable)"}
        });
    } else {
        Logger::instance().component(VerbosityLevel::INFO, "Grounding", {
            {"lower bound", std::to_string(result.lower_bound)}
        });
    }

    // Build the grounded problem.
    result.grounded_problem = lifted_problem_.with_actions(std::move(ground_actions));

    return result;
}

} // namespace rantanplan
