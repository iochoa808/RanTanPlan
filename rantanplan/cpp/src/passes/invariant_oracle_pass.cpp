#include "invariant_oracle_pass.hpp"
#include "../config/config.hpp"
#include "../util/logger.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/stats.hpp"
#include <algorithm>
#include <cmath>

namespace rantanplan {

void InvariantOraclePass::apply(PipelineResult& result) const {
    auto& config = Config::instance();
    if (!config.local_reasoning.enabled) return;

    ScopedTimer timer("pass.InvariantOracle.time_ms");
    const auto& problem = result.problem;
    StateConstraints invariants;

    // Phase A: RPG bound injection
    if (config.local_reasoning.rpg_bounds) {
        // Use the fixpoint RPG from GoalRelevancePass if available,
        // otherwise build a quick one.
        NumericRelaxedPlanningGraph* rpg_ptr = result.fixpoint_rpg.get();
        std::unique_ptr<NumericRelaxedPlanningGraph> local_rpg;
        if (!rpg_ptr) {
            local_rpg = std::make_unique<NumericRelaxedPlanningGraph>(problem);
            local_rpg->build();
            rpg_ptr = local_rpg.get();
        }
        if (rpg_ptr) {
            invariants.bounds = collect_rpg_bounds(*rpg_ptr, problem);
            Logger::instance().component(VerbosityLevel::INFO, "InvOracle", {
                {"rpg_bounds", std::to_string(invariants.bounds.size())}
            });
        }
    }

    // Phase B+C: Mutex candidate generation + verification
    if (config.local_reasoning.mutex_discovery) {
        auto candidates = generate_mutex_candidates(problem);

        // Build initial-state lookup for boolean fluents
        std::unordered_set<ExprID> initially_true;
        for (const auto& assignment : problem.initial_state()) {
            ExprID fid = assignment.fluent_id();
            ExprID vid = assignment.value_id();
            if (problem.is_bool_type(fid) && problem.pool().is_true_constant(vid)) {
                initially_true.insert(fid);
            }
        }

        int verified_tier1 = 0;
        int verified_h2 = 0;
        int verified_exo = 0;

        // Tier 1: syntactic check (microseconds, no fixpoint needed)
        std::vector<const MutexCandidate*> tier1_failed;
        for (const auto& candidate : candidates) {
            if (syntactic_mutex_check(candidate, problem)) {
                MutexConstraint inv;
                inv.members = candidate.members;
                inv.label = candidate.predicate + "(" + candidate.entity + ", ...)";

                if (syntactic_exactly_one_check(candidate, problem, initially_true)) {
                    inv.exactly_one = true;
                    verified_exo++;
                } else {
                    verified_tier1++;
                }

                Logger::instance().info("  mutex " +
                    std::string(inv.exactly_one ? "exactly-one" : "at-most-one") +
                    " [tier1]: " + inv.label + " (" +
                    std::to_string(inv.members.size()) + " members)");
                invariants.mutexes.push_back(std::move(inv));
            } else {
                tier1_failed.push_back(&candidate);
            }
        }

        // Tier 1.5: h² fixpoint for candidates that failed Tier 1.
        // Only run if there are failed candidates (avoid building the checker
        // when Tier 1 handled everything).
        if (!tier1_failed.empty()) {
            H2MutexChecker h2(problem);
            h2.compute();

            for (const auto* candidate : tier1_failed) {
                if (h2.all_pairs_mutex(candidate->members)) {
                    MutexConstraint inv;
                    inv.members = candidate->members;
                    inv.label = candidate->predicate + "(" + candidate->entity + ", ...)";

                    if (syntactic_exactly_one_check(*candidate, problem, initially_true)) {
                        inv.exactly_one = true;
                        verified_exo++;
                    } else {
                        verified_h2++;
                    }

                    Logger::instance().info("  mutex " +
                        std::string(inv.exactly_one ? "exactly-one" : "at-most-one") +
                        " [h2]: " + inv.label + " (" +
                        std::to_string(inv.members.size()) + " members)");
                    invariants.mutexes.push_back(std::move(inv));
                }
            }

            Stats::instance().set("inv_oracle.h2_mutex_pairs",
                                  static_cast<double>(h2.mutex_pair_count()));
        }

        int total_amo = verified_tier1 + verified_h2;
        Logger::instance().component(VerbosityLevel::INFO, "InvOracle", {
            {"mutex_candidates", std::to_string(candidates.size())},
            {"tier1_amo", std::to_string(verified_tier1)},
            {"h2_amo", std::to_string(verified_h2)},
            {"exactly_one", std::to_string(verified_exo)}
        });

        Stats::instance().set("inv_oracle.mutex_candidates",
                              static_cast<double>(candidates.size()));
        Stats::instance().set("inv_oracle.mutex_amo",
                              static_cast<double>(total_amo));
        Stats::instance().set("inv_oracle.mutex_exo",
                              static_cast<double>(verified_exo));
    }

    Stats::instance().set("inv_oracle.rpg_bounds",
                          static_cast<double>(invariants.bounds.size()));

    result.state_constraints = std::move(invariants);
}

// ============================================================================
// RPG Bound Collection
// ============================================================================

std::vector<BoundConstraint> InvariantOraclePass::collect_rpg_bounds(
    const NumericRelaxedPlanningGraph& rpg,
    const Problem& problem) const {

    std::vector<BoundConstraint> bounds;
    auto rpg_bounds = rpg.get_state_variable_bounds();

    for (const auto& [eid, interval] : rpg_bounds) {
        // Only emit bounds for numeric fluents (skip boolean [1,1] entries)
        if (!problem.is_numeric_type(eid)) continue;

        if (std::isfinite(interval.lower)) {
            bounds.push_back({eid, interval.lower, true});
        }
        if (std::isfinite(interval.upper)) {
            bounds.push_back({eid, interval.upper, false});
        }
    }
    return bounds;
}

// ============================================================================
// Mutex Candidate Generation
// ============================================================================

std::vector<InvariantOraclePass::MutexCandidate>
InvariantOraclePass::generate_mutex_candidates(const Problem& problem) const {
    const auto& pool = problem.pool();

    // Step 1: Identify which boolean fluents are modified by some action.
    std::unordered_set<ExprID> modified_fluents;
    for (const auto& action : problem.actions()) {
        for (const auto& effect : action.effects()) {
            ExprID fid = effect.effect_expression().fluent_id();
            if (problem.is_bool_type(fid)) {
                modified_fluents.insert(fid);
            }
        }
    }

    // Step 2: Group grounded boolean fluents by (head_symbol, argument_position, argument_value).
    // Key: (fluent_symbol_id, arg_position) -> (arg_value_id -> list of fluent ExprIDs)
    struct GroupKey {
        ExprID head;
        size_t arg_pos;
        bool operator==(const GroupKey& o) const { return head == o.head && arg_pos == o.arg_pos; }
    };
    struct GroupKeyHash {
        size_t operator()(const GroupKey& k) const {
            return std::hash<int32_t>()(k.head.id) ^ (std::hash<size_t>()(k.arg_pos) << 16);
        }
    };

    // Map: GroupKey -> (argument_ExprID -> list of grounded fluent ExprIDs)
    std::unordered_map<GroupKey,
                       std::unordered_map<ExprID, std::vector<ExprID>>,
                       GroupKeyHash> groups;

    for (ExprID fid : problem.grounded_fluents()) {
        if (!problem.is_bool_type(fid)) continue;
        if (!pool.is_state_variable(fid)) continue;
        if (modified_fluents.find(fid) == modified_fluents.end()) continue;

        size_t arity = pool.argument_count(fid);
        if (arity < 2) continue;  // Need at least 2 args to group by position

        ExprID head = pool.head_symbol_id(fid);

        for (size_t pos = 0; pos < arity; ++pos) {
            ExprID arg_val = pool.argument(fid, pos);
            groups[{head, pos}][arg_val].push_back(fid);
        }
    }

    // Step 3: Extract candidates (groups with 2-20 members)
    std::vector<MutexCandidate> candidates;
    for (const auto& [gkey, arg_groups] : groups) {
        for (const auto& [arg_val, members] : arg_groups) {
            if (members.size() < 2 || members.size() > 20) continue;

            MutexCandidate c;
            c.members = members;
            c.predicate = pool.to_string(gkey.head);
            c.entity = pool.to_string(arg_val);
            candidates.push_back(std::move(c));
        }
    }

    // Cap at max candidates (prioritize larger groups — they prune more)
    if (candidates.size() > 50) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const MutexCandidate& a, const MutexCandidate& b) {
                      return a.members.size() > b.members.size();
                  });
        candidates.resize(50);
    }

    return candidates;
}

// ============================================================================
// Tier 1 Syntactic Mutex Check
// ============================================================================

bool InvariantOraclePass::syntactic_mutex_check(
    const MutexCandidate& candidate,
    const Problem& problem) const {

    const auto& members = candidate.members;
    std::unordered_set<ExprID> member_set(members.begin(), members.end());

    // For each action that modifies any member of the group:
    // 1. It must not have conditional effects on any member.
    // 2. If it sets a member to TRUE, it must set exactly one other member to FALSE.
    // 3. It must not set more than one member to TRUE.

    for (const auto& action : problem.actions()) {
        bool touches_group = false;
        int adds_count = 0;       // members set to TRUE
        int deletes_count = 0;    // members set to FALSE
        bool has_conditional_on_member = false;

        for (const auto& effect : action.effects()) {
            const auto& ee = effect.effect_expression();
            ExprID fid = ee.fluent_id();

            if (member_set.find(fid) == member_set.end()) continue;
            touches_group = true;

            // Conditional effects on group members disqualify Tier 1
            if (ee.is_conditional()) {
                has_conditional_on_member = true;
                break;
            }

            // For boolean effects: ASSIGN with value TRUE = add, ASSIGN with value FALSE = delete
            if (ee.is_assign()) {
                ExprID val = ee.value_id();
                if (problem.pool().is_true_constant(val)) {
                    adds_count++;
                } else if (problem.pool().is_false_constant(val)) {
                    deletes_count++;
                } else {
                    // Non-constant boolean assignment — can't verify syntactically
                    return false;
                }
            } else {
                // INCREASE/DECREASE on a boolean? Shouldn't happen, but reject
                return false;
            }
        }

        if (!touches_group) continue;
        if (has_conditional_on_member) return false;

        // Sufficient conditions:
        // - If the action adds members, it must add exactly 1 and delete exactly 1.
        // - If the action only deletes members (no adds), that's fine for at-most-one
        //   (cardinality can only decrease).
        if (adds_count > 0) {
            if (adds_count != 1 || deletes_count != 1) {
                return false;
            }
        }
        // If adds_count == 0 and deletes_count > 0: action only removes members,
        // which cannot violate at-most-one. This is fine.
    }

    return true;
}

// ============================================================================
// Exactly-One Upgrade (Syntactic)
// ============================================================================

bool InvariantOraclePass::syntactic_exactly_one_check(
    const MutexCandidate& candidate,
    const Problem& problem,
    const std::unordered_set<ExprID>& initially_true) const {

    const auto& members = candidate.members;
    std::unordered_set<ExprID> member_set(members.begin(), members.end());

    // Check: exactly one member is TRUE in initial state
    int initial_true_count = 0;
    for (ExprID m : members) {
        if (initially_true.count(m)) {
            initial_true_count++;
        }
    }
    if (initial_true_count != 1) return false;

    // Check: every action that deletes a member also adds a member.
    // (Combined with at-most-one, this means the cardinality never decreases.)
    for (const auto& action : problem.actions()) {
        bool deletes_member = false;
        bool adds_member = false;

        for (const auto& effect : action.effects()) {
            const auto& ee = effect.effect_expression();
            ExprID fid = ee.fluent_id();
            if (member_set.find(fid) == member_set.end()) continue;

            if (ee.is_assign()) {
                ExprID val = ee.value_id();
                if (problem.pool().is_true_constant(val)) {
                    adds_member = true;
                } else if (problem.pool().is_false_constant(val)) {
                    deletes_member = true;
                }
            }
        }

        if (deletes_member && !adds_member) {
            return false;  // Can reduce cardinality below 1
        }
    }

    return true;
}

} // namespace rantanplan
