#include "reachability_grounder.hpp"
#include "binding_matcher.hpp"
#include "action_instantiator.hpp"
#include "../util/logger.hpp"
#include "../util/scoped_timer.hpp"
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

/// Hash a fingerprint vector (FNV-1a).
static uint64_t hash_fingerprint(const std::vector<int>& fp) {
    uint64_t h = 14695981039346656037ULL;
    for (int v : fp) {
        h ^= static_cast<uint64_t>(static_cast<uint32_t>(v));
        h *= 1099511628211ULL;
    }
    return h;
}

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
// goals_reachable
// ---------------------------------------------------------------------------

bool ReachabilityGrounder::goals_reachable(const FactIndex& facts) const {
    const auto& pool = lifted_problem_.pool();

    for (const auto& goal : lifted_problem_.goals()) {
        ExprID gid = goal.goal_id();
        if (!gid.valid()) continue;

        // Only check boolean fluent atoms at top level.
        // Walk through ANDs.
        // For complex goals (OR, numeric), we can't prove unreachability easily,
        // so we skip them (assume satisfiable).
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
            }
            // NOT, OR, numeric comparisons, etc. — skip (assume satisfiable).
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

    // 1. Initialize FactIndex from initial state.
    FactIndex facts(lifted_problem_);
    facts.initialize_from_initial_state();

    Logger::instance().info("Grounding: initial facts = " + std::to_string(facts.total_fact_count()));
    Logger::instance().info("Grounding: action schemas = " + std::to_string(lifted_problem_.action_count()));

    // Per-schema seen-binding sets for deduplication.
    std::vector<std::unordered_set<uint64_t>> seen_hashes(lifted_problem_.action_count());

    // Accumulate all ground actions across iterations.
    std::vector<Action> ground_actions;

    int iteration = 0;
    int goal_reachable_layer = -1;
    bool changed = true;

    while (changed) {
        changed = false;
        ++iteration;

        BindingMatcher matcher(lifted_problem_, facts);

        size_t new_actions_this_iter = 0;
        size_t new_facts_this_iter   = 0;

        for (size_t schema_idx = 0; schema_idx < lifted_problem_.action_count(); ++schema_idx) {
            const Action& schema = lifted_problem_.action(schema_idx);

            auto bindings = matcher.find_bindings(schema);

            for (auto& binding : bindings) {
                auto fp = binding_fingerprint(binding, schema.parameter_count());
                uint64_t h = hash_fingerprint(fp);

                if (!seen_hashes[schema_idx].insert(h).second) {
                    continue;  // Already instantiated this binding.
                }

                // Instantiate the ground action.
                Action ga = instantiate_action(pool, lifted_problem_, schema, binding);
                size_t nf = collect_add_effects(ga, facts);
                if (nf > 0) {
                    new_facts_this_iter += nf;
                    changed = true;
                }
                ground_actions.push_back(std::move(ga));
                ++new_actions_this_iter;
            }
        }

        // Check goal reachability at this layer.
        if (goal_reachable_layer < 0 && goals_reachable(facts)) {
            goal_reachable_layer = iteration;
        }

        Logger::instance().info("Grounding: iteration " + std::to_string(iteration) +
                      " — new actions=" + std::to_string(new_actions_this_iter) +
                      ", new facts=" + std::to_string(new_facts_this_iter) +
                      ", total facts=" + std::to_string(facts.total_fact_count()));
    }

    // Build result.
    GroundingResult result;
    result.iterations          = iteration;
    result.ground_action_count = ground_actions.size();
    result.lower_bound         = std::max(0, goal_reachable_layer);
    result.proven_unsolvable   = (goal_reachable_layer < 0);

    Logger::instance().info("Grounding: fixpoint after " + std::to_string(iteration) +
                  " iterations, " + std::to_string(ground_actions.size()) +
                  " ground actions, " + std::to_string(facts.total_fact_count()) +
                  " reachable facts");

    if (result.proven_unsolvable) {
        Logger::instance().info("Grounding: goal UNREACHABLE — problem proven unsolvable");
    } else {
        Logger::instance().info("Grounding: lower bound = " + std::to_string(result.lower_bound));
    }

    // Build the grounded problem.
    result.grounded_problem = lifted_problem_.with_actions(std::move(ground_actions));

    return result;
}

} // namespace rantanplan
