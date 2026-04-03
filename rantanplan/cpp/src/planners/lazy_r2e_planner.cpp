#include "lazy_r2e_planner.hpp"
#include "../config/config.hpp"
#include "../analysis/numeric_relaxed_planning_graph.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include "../util/memory_tracker.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

namespace rantanplan {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LazyR2EPlanner::LazyR2EPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
    : BasePlanner(problem, encoder, ctx) {
    build_action_metadata();
}

void LazyR2EPlanner::build_action_metadata() {
    // Discover which fluents each action modifies and which fluents are modifiable.
    for (const Action& action : problem_.actions()) {
        auto& by_fluent = action_effects_by_fluent_[&action];
        for (const Effect& effect : action.effects()) {
            ExprID fid = effect.effect_expression().fluent_id();
            by_fluent[fid].push_back(&effect);
            modifiable_fluents_.insert(fid);
        }
    }
}

void LazyR2EPlanner::compute_action_ordering() {
    // Try RPG ordering; fall back to declaration order.
    NumericRelaxedPlanningGraph rpg(problem_);
    if (rpg.build()) {
        auto ordered = rpg.get_action_ordering();
        if (!ordered.empty()) {
            Logger::instance().info("Lazy R2E: using RPG ordering (" +
                std::to_string(ordered.size()) + " actions)");
            action_ordering_ = std::move(ordered);
        }
    }
    if (action_ordering_.empty()) {
        Logger::instance().info("Lazy R2E: using declaration ordering (" +
            std::to_string(problem_.actions().size()) + " actions)");
        action_ordering_.reserve(problem_.actions().size());
        for (const Action& a : problem_.actions()) action_ordering_.push_back(&a);
    }
    // Build rank map for fast sorting during replenishment.
    for (size_t i = 0; i < action_ordering_.size(); ++i) {
        action_rank_[action_ordering_[i]] = i;
    }
}

// ---------------------------------------------------------------------------
// Chain building
// ---------------------------------------------------------------------------

size_t LazyR2EPlanner::append_slot(const Action* action, bool blocked) {
    int sid = next_slot_id_++;
    std::string suffix = "_s" + std::to_string(sid);

    // Action variable
    z3::expr action_var = ctx_.bool_const(("act_" + action->name() + suffix).c_str());

    // Blocking literal
    z3::expr blocking_lit = ctx_.bool_const(("blk" + suffix).c_str());
    if (blocked) {
        solver_.add(z3::implies(blocking_lit, !action_var));
    }

    // Create the slot (chain_vars populated below)
    chain_.push_back({action, sid, !blocked, action_var, blocking_lit, {}});
    ActionSlot& slot = chain_.back();
    size_t slot_idx = chain_.size() - 1;

    if (blocked) {
        block_id_to_chain_index_[blocking_lit.id()] = slot_idx;
    }

    // --- Encode chain equations and precondition ---

    // Build prev substitution: fluent@0 → current chain tail
    z3::expr_vector sub_from(ctx_), sub_to(ctx_);
    build_substitution_arrays(sub_from, sub_to);

    // Precondition: action_var → precond(σ_prev)
    if (action->has_precondition()) {
        z3::expr precond = encoder_.convert_expr_id_to_z3(action->precondition_id(), 0);
        z3::expr sub_precond = sub_from.empty() ? precond : precond.substitute(sub_from, sub_to);
        solver_.add(z3::implies(action_var, sub_precond));
    }

    // Chain equations per modified fluent
    auto it = action_effects_by_fluent_.find(action);
    if (it != action_effects_by_fluent_.end()) {
        for (const auto& [fluent_id, effects] : it->second) {
            // Previous value in chain for this fluent
            z3::expr prev_value = var_info_.count(fluent_id)
                ? var_info_.at(fluent_id).chain_tail
                : encoder_.convert_expr_id_to_z3(fluent_id, 0);

            // Create chain variable
            std::string chain_name = problem_.pool().to_string(fluent_id) + suffix;
            z3::expr chain_var = encoder_.get_variable_factory().create_symbol_variable(
                chain_name, problem_.type_for_id(fluent_id));

            slot.chain_vars.emplace(fluent_id, chain_var);

            // Compute executed value
            z3::expr executed = compute_effect_value(effects, prev_value, sub_from, sub_to);

            // Chain equation: chain_var = ite(action_var, executed, prev)
            solver_.add(chain_var == z3::ite(action_var, executed, prev_value));

            // Update chain tail
            if (var_info_.count(fluent_id)) {
                var_info_.at(fluent_id).chain_tail = chain_var;
            } else {
                var_info_.emplace(fluent_id, VarChainInfo{chain_var});
            }
        }
    }

    return slot_idx;
}

void LazyR2EPlanner::build_substitution_arrays(z3::expr_vector& from, z3::expr_vector& to) {
    for (const auto& [fid, info] : var_info_) {
        from.push_back(encoder_.convert_expr_id_to_z3(fid, 0));
        to.push_back(info.chain_tail);
    }
}

z3::expr LazyR2EPlanner::compute_effect_value(
    const std::vector<const Effect*>& effects,
    const z3::expr& prev_value,
    const z3::expr_vector& sub_from, const z3::expr_vector& sub_to) {

    // Partition into unconditional and conditional (same as R2E encoder)
    std::vector<const Effect*> unconditional, conditional;
    for (const Effect* eff : effects) {
        if (eff->is_conditional()) conditional.push_back(eff);
        else unconditional.push_back(eff);
    }

    z3::expr result = prev_value;

    // Unconditional: accumulate (ASSIGN → last wins; INCREASE/DECREASE → sum)
    for (const Effect* eff : unconditional) {
        result = create_effect_value_z3(eff->effect_expression(), result, sub_from, sub_to);
    }

    // Conditional: ITE per effect
    for (const Effect* eff : conditional) {
        z3::expr cond = encoder_.convert_expr_id_to_z3(eff->effect_expression().condition_id(), 0);
        z3::expr sub_cond = sub_from.empty() ? cond : cond.substitute(sub_from, sub_to);

        z3::expr with_effect = create_effect_value_z3(
            eff->effect_expression(), result, sub_from, sub_to);
        result = z3::ite(sub_cond, with_effect, result);
    }

    return result;
}

z3::expr LazyR2EPlanner::create_effect_value_z3(
    const EffectExpression& eff_expr,
    const z3::expr& running_value,
    const z3::expr_vector& sub_from, const z3::expr_vector& sub_to) {

    z3::expr value_z3 = encoder_.convert_expr_id_to_z3(eff_expr.value_id(), 0);
    z3::expr sub_value = sub_from.empty() ? value_z3 : value_z3.substitute(sub_from, sub_to);

    switch (eff_expr.kind()) {
        case EffectExpression::Kind::ASSIGN:   return sub_value;
        case EffectExpression::Kind::INCREASE: return running_value + sub_value;
        case EffectExpression::Kind::DECREASE: return running_value - sub_value;
    }
    return sub_value;
}

// ---------------------------------------------------------------------------
// Goal management
// ---------------------------------------------------------------------------

void LazyR2EPlanner::setup_goal_assumptions() {
    refresh_goal_assumptions();
}

void LazyR2EPlanner::refresh_goal_assumptions() {
    // Deactivate all current goal assumptions.
    for (auto& ga : goal_assumptions_) ga.active = false;

    int version = next_goal_version_++;

    // Build substitution: fluent@0 → current chain tail
    z3::expr_vector sub_from(ctx_), sub_to(ctx_);
    build_substitution_arrays(sub_from, sub_to);

    for (size_t i = 0; i < problem_.goals().size(); ++i) {
        const Goal& g = problem_.goals()[i];
        std::string lit_name = "goal_" + std::to_string(i)
                             + "_v" + std::to_string(version);
        z3::expr goal_lit = ctx_.bool_const(lit_name.c_str());

        z3::expr goal_z3 = encoder_.convert_expr_id_to_z3(g.goal_id(), 0);
        z3::expr sub_goal = sub_from.empty() ? goal_z3
                          : goal_z3.substitute(sub_from, sub_to);

        solver_.add(z3::implies(goal_lit, sub_goal));
        goal_assumptions_.push_back({goal_lit, true});
    }
}

// ---------------------------------------------------------------------------
// Search loop helpers
// ---------------------------------------------------------------------------

z3::expr_vector LazyR2EPlanner::build_assumptions() {
    z3::expr_vector assumptions(ctx_);

    // Blocking literals for inactive slots
    for (const auto& slot : chain_) {
        if (!slot.activated) {
            assumptions.push_back(slot.blocking_lit);
        }
    }

    // Active goal assumptions
    for (const auto& ga : goal_assumptions_) {
        if (ga.active) {
            assumptions.push_back(ga.lit);
        }
    }

    return assumptions;
}

int LazyR2EPlanner::process_core(const z3::expr_vector& core) {
    int activated = 0;
    std::vector<const Action*> to_replenish;

    for (unsigned i = 0; i < core.size(); ++i) {
        unsigned eid = core[i].id();
        auto it = block_id_to_chain_index_.find(eid);
        if (it != block_id_to_chain_index_.end()) {
            size_t idx = it->second;
            chain_[idx].activated = true;
            block_id_to_chain_index_.erase(it);
            to_replenish.push_back(chain_[idx].action);
            activated++;
        }
    }

    // Sort replenished actions by ARPG order: enablers first, goal-achievers last.
    // This ensures that when these slots are later activated, goal-achieving actions
    // see a richer cumulative state from preceding enablers.
    std::sort(to_replenish.begin(), to_replenish.end(),
        [this](const Action* a, const Action* b) {
            return action_rank_.at(a) < action_rank_.at(b);
        });

    // Replenish: append fresh blocked copies at the tail
    for (const Action* action : to_replenish) {
        append_slot(action, /*blocked=*/true);
    }

    // If any chain tails changed, refresh goal assumptions
    if (activated > 0) {
        refresh_goal_assumptions();
    }

    return activated;
}

void LazyR2EPlanner::extend_chain() {
    // Append a fresh blocked copy of every action to the chain tail.
    for (const Action* action : action_ordering_) {
        if (!action->effects().empty()) {
            append_slot(action, /*blocked=*/true);
        }
    }
    refresh_goal_assumptions();
}

// ---------------------------------------------------------------------------
// Plan extraction
// ---------------------------------------------------------------------------

Plan LazyR2EPlanner::extract_plan(const z3::model& model) {
    Plan plan;
    for (const auto& slot : chain_) {
        if (!slot.activated) continue;  // blocked → definitely false
        z3::expr val = model.eval(slot.action_var, true);
        if (val.is_true()) {
            plan.add_action(slot.action);
        }
    }
    return plan;
}

// ---------------------------------------------------------------------------
// Main search
// ---------------------------------------------------------------------------

Plan LazyR2EPlanner::search() {
    auto& config = Config::instance();
    auto& stats = Stats::instance();

    solution_found_ = false;
    timed_out_ = false;
    init_deadline();

    Logger::instance().info("Starting Lazy R2E search, timeout: " + format_timeout_string());

    // 1. Encode initial state at timestep 0
    solver_.add(*encoder_.encode_initial_state());

    // 2. Initialize chain tails to timestep-0 variables for all modifiable fluents
    for (ExprID fid : modifiable_fluents_) {
        z3::expr base_var = encoder_.convert_expr_id_to_z3(fid, 0);
        var_info_.emplace(fid, VarChainInfo{base_var});
    }

    // 3. Compute action ordering once (ARPG-based)
    compute_action_ordering();

    // 4. Build initial frontier: one blocked slot per action (ARPG order)
    int actions_with_effects = 0;
    for (const Action* action : action_ordering_) {
        if (!action->effects().empty()) {
            append_slot(action, /*blocked=*/true);
            actions_with_effects++;
        }
    }

    Logger::instance().info("Initial frontier: " + std::to_string(actions_with_effects) +
        " action slots, " + std::to_string(var_info_.size()) + " tracked variables, " +
        std::to_string(next_slot_id_) + " chain slots");

    // 5. Setup goal assumptions
    setup_goal_assumptions();

    // 6. Search loop
    int max_chain_length = config.planner.max_steps * static_cast<int>(problem_.actions().size());
    int round = 0;
    int extensions = 0;
    double total_time = 0.0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        if (!apply_solver_timeout(solver_)) break;

        auto round_start = std::chrono::high_resolution_clock::now();

        z3::expr_vector assumptions = build_assumptions();

        auto solve_start = std::chrono::high_resolution_clock::now();
        z3::check_result result = solver_.check(assumptions);
        auto solve_end = std::chrono::high_resolution_clock::now();
        double solve_time = std::chrono::duration<double>(solve_end - solve_start).count();

        auto round_end = std::chrono::high_resolution_clock::now();
        double round_time = std::chrono::duration<double>(round_end - round_start).count();
        total_time = std::chrono::duration<double>(round_end - start_time).count();

        // Count active (blocked) slots
        size_t blocked_count = 0;
        for (const auto& s : chain_) if (!s.activated) blocked_count++;
        size_t active_count = chain_.size() - blocked_count;

        double mem = MemoryTracker::instance().get_current_memory_mb();

        Logger::instance().timestep_solving(VerbosityLevel::INFO, round, {
            {"solve", std::to_string(solve_time) + "s"},
            {"round", std::to_string(round_time) + "s"},
            {"slots", std::to_string(chain_.size())},
            {"active", std::to_string(active_count)},
            {"blocked", std::to_string(blocked_count)},
            {"mem", std::to_string(static_cast<int>(mem)) + "MB"}
        }, "R");

        stats.add("planner.solve_time", solve_time);
        stats.add("planner.total_time", round_time);

        if (result == z3::sat) {
            z3::model model = solver_.get_model();
            solution_found_ = true;

            Plan plan = extract_plan(model);

            Logger::instance().info("\n*** PLAN FOUND: " +
                std::to_string(plan.length()) + " actions, " +
                std::to_string(round) + " rounds, " +
                std::to_string(extensions) + " extensions " +
                "(total time: " + std::to_string(total_time) + "s) ***");

            stats.set("planner.plan_length", static_cast<double>(plan.length()));
            stats.set("planner.rounds", static_cast<double>(round));
            stats.set("planner.extensions", static_cast<double>(extensions));

            plan.write_ipc(config.planner.output_plan, 1,
                           -1.0, true,
                           config.planner.strategy, config.planner.mode, total_time);

            collect_statistics();
            return plan;

        } else if (result == z3::unsat) {
            z3::expr_vector core = solver_.unsat_core();

            int activated = process_core(core);

            if (config.is_verbose() || config.is_debug()) {
                Logger::instance().info("  Core size: " + std::to_string(core.size()) +
                    ", activated: " + std::to_string(activated));
            }

            if (activated == 0) {
                // No blocking literals in core → current capacity insufficient
                if (static_cast<int>(chain_.size()) >= max_chain_length) {
                    Logger::instance().info("Chain length limit reached (" +
                        std::to_string(chain_.size()) + ")");
                    break;
                }
                extensions++;
                Logger::instance().info("Extending chain (extension #" +
                    std::to_string(extensions) + ", chain size: " +
                    std::to_string(chain_.size()) + ")");
                extend_chain();
            }

        } else {
            if (handle_unknown_result(solver_, "round " + std::to_string(round))) break;
        }

        round++;
    }

    if (timed_out_) {
        Logger::instance().info("No plan found (timeout).");
    } else {
        Logger::instance().info("\n*** NO PLAN FOUND within chain limit ***");
    }

    collect_statistics();
    return Plan();
}

} // namespace rantanplan
