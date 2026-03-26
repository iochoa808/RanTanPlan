#include "causal_lazy_r2e_planner.hpp"
#include "../config/config.hpp"
#include "../arpg/arpg.hpp"
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

CausalLazyR2EPlanner::CausalLazyR2EPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
    : BasePlanner(problem, encoder, ctx) {
    build_action_metadata();
}

void CausalLazyR2EPlanner::build_action_metadata() {
    for (const Action& action : problem_.actions()) {
        auto& by_fluent = action_effects_by_fluent_[&action];
        for (const Effect& effect : action.effects()) {
            ExprID fid = effect.effect_expression().fluent_id();
            by_fluent[fid].push_back(&effect);
            modifiable_fluents_.insert(fid);
        }
    }
}

void CausalLazyR2EPlanner::compute_action_ordering() {
    ARPG arpg(problem_);
    if (arpg.construct_graph()) {
        auto ordered = arpg.get_action_ordering();
        if (!ordered.empty()) {
            Logger::instance().info("Causal Lazy R2E: using ARPG ordering (" +
                std::to_string(ordered.size()) + " actions)");
            action_ordering_ = std::move(ordered);
        }
    }
    if (action_ordering_.empty()) {
        Logger::instance().info("Causal Lazy R2E: using declaration ordering (" +
            std::to_string(problem_.actions().size()) + " actions)");
        action_ordering_.reserve(problem_.actions().size());
        for (const Action& a : problem_.actions()) action_ordering_.push_back(&a);
    }
    for (size_t i = 0; i < action_ordering_.size(); ++i) {
        action_rank_[action_ordering_[i]] = i;
    }
}

// ---------------------------------------------------------------------------
// Achiever setup
// ---------------------------------------------------------------------------

void CausalLazyR2EPlanner::build_action_id_map() {
    for (const Action& a : problem_.actions()) {
        action_id_to_ptr_[a.id()] = &a;
    }
}

void CausalLazyR2EPlanner::build_condition_achiever_cache() {
    // Translate AchieversAnalysis results (Action by value) to const Action* via id map.
    for (ExprID cond : achievers_->get_all_conditions()) {
        const auto& achiever_actions = achievers_->get_achievers(cond);
        auto& ptrs = condition_achievers_[cond];
        ptrs.reserve(achiever_actions.size());
        for (const Action& a : achiever_actions) {
            auto it = action_id_to_ptr_.find(a.id());
            if (it != action_id_to_ptr_.end()) {
                ptrs.push_back(it->second);
            }
        }
    }

    // Extract goal condition ExprIDs.
    for (ExprID cond : achievers_->get_goal_conditions()) {
        goal_condition_ids_.push_back(cond);
    }

    // Extract precondition ExprIDs per action.
    for (const Action& a : problem_.actions()) {
        const auto& precs = achievers_->get_preconditions(a);
        if (!precs.empty()) {
            auto* ptr = action_id_to_ptr_[a.id()];
            auto& vec = action_precondition_ids_[ptr];
            vec.assign(precs.begin(), precs.end());
        }
    }
}

void CausalLazyR2EPlanner::compute_init_satisfied_conditions() {
    // Create a temporary solver with just the initial state, get a model,
    // and evaluate each condition to determine which are already true.
    z3::solver init_solver(ctx_);
    init_solver.add(*encoder_.encode_initial_state());

    if (init_solver.check() != z3::sat) return;
    z3::model init_model = init_solver.get_model();

    for (ExprID cond : achievers_->get_all_conditions()) {
        z3::expr cond_z3 = encoder_.convert_expr_id_to_z3(cond, 0);
        z3::expr val = init_model.eval(cond_z3, true);
        if (val.is_true()) {
            init_satisfied_conditions_.insert(cond);
        }
    }
}

void CausalLazyR2EPlanner::initialize_achievers() {
    auto init_start = std::chrono::high_resolution_clock::now();

    build_action_id_map();

    achievers_ = std::make_unique<AchieversAnalysis>(problem_);

    build_condition_achiever_cache();
    compute_init_satisfied_conditions();

    auto init_end = std::chrono::high_resolution_clock::now();
    double init_time = std::chrono::duration<double>(init_end - init_start).count();

    Logger::instance().info("Causal R2E achiever analysis: " +
        std::to_string(condition_achievers_.size()) + " conditions, " +
        std::to_string(init_satisfied_conditions_.size()) + " init-satisfied, " +
        std::to_string(goal_condition_ids_.size()) + " goal conditions " +
        "(" + std::to_string(init_time) + "s)");
}

// ---------------------------------------------------------------------------
// Chain building
// ---------------------------------------------------------------------------

size_t CausalLazyR2EPlanner::append_slot(const Action* action, bool blocked) {
    int sid = next_slot_id_++;
    std::string suffix = "_s" + std::to_string(sid);

    z3::expr action_var = ctx_.bool_const(("act_" + action->name() + suffix).c_str());

    z3::expr blocking_lit = ctx_.bool_const(("blk" + suffix).c_str());
    if (blocked) {
        solver_.add(z3::implies(blocking_lit, !action_var));
    }

    chain_.push_back({action, sid, !blocked, action_var, blocking_lit, {}});
    ActionSlot& slot = chain_.back();
    size_t slot_idx = chain_.size() - 1;

    if (blocked) {
        block_id_to_chain_index_[blocking_lit.id()] = slot_idx;
    }

    // Build prev substitution
    z3::expr_vector sub_from(ctx_), sub_to(ctx_);
    build_substitution_arrays(sub_from, sub_to);

    // Precondition
    if (action->has_precondition()) {
        z3::expr precond = encoder_.convert_expr_id_to_z3(action->precondition_id(), 0);
        z3::expr sub_precond = sub_from.empty() ? precond : precond.substitute(sub_from, sub_to);
        solver_.add(z3::implies(action_var, sub_precond));
    }

    // Chain equations per modified fluent
    auto it = action_effects_by_fluent_.find(action);
    if (it != action_effects_by_fluent_.end()) {
        for (const auto& [fluent_id, effects] : it->second) {
            z3::expr prev_value = var_info_.count(fluent_id)
                ? var_info_.at(fluent_id).chain_tail
                : encoder_.convert_expr_id_to_z3(fluent_id, 0);

            std::string chain_name = problem_.pool().to_string(fluent_id) + suffix;
            z3::expr chain_var = encoder_.get_variable_factory().create_symbol_variable(
                chain_name, problem_.type_for_id(fluent_id));

            slot.chain_vars.emplace(fluent_id, chain_var);

            z3::expr executed = compute_effect_value(effects, prev_value, sub_from, sub_to);
            solver_.add(chain_var == z3::ite(action_var, executed, prev_value));

            if (var_info_.count(fluent_id)) {
                var_info_.at(fluent_id).chain_tail = chain_var;
            } else {
                var_info_.emplace(fluent_id, VarChainInfo{chain_var});
            }
        }
    }

    // Add precondition achiever disjunctions for this slot
    add_precondition_achiever_disjunctions(slot_idx);

    return slot_idx;
}

void CausalLazyR2EPlanner::build_substitution_arrays(z3::expr_vector& from, z3::expr_vector& to) {
    for (const auto& [fid, info] : var_info_) {
        from.push_back(encoder_.convert_expr_id_to_z3(fid, 0));
        to.push_back(info.chain_tail);
    }
}

z3::expr CausalLazyR2EPlanner::compute_effect_value(
    const std::vector<const Effect*>& effects,
    const z3::expr& prev_value,
    const z3::expr_vector& sub_from, const z3::expr_vector& sub_to) {

    std::vector<const Effect*> unconditional, conditional;
    for (const Effect* eff : effects) {
        if (eff->is_conditional()) conditional.push_back(eff);
        else unconditional.push_back(eff);
    }

    z3::expr result = prev_value;

    for (const Effect* eff : unconditional) {
        result = create_effect_value_z3(eff->effect_expression(), result, sub_from, sub_to);
    }

    for (const Effect* eff : conditional) {
        z3::expr cond = encoder_.convert_expr_id_to_z3(eff->effect_expression().condition_id(), 0);
        z3::expr sub_cond = sub_from.empty() ? cond : cond.substitute(sub_from, sub_to);

        z3::expr with_effect = create_effect_value_z3(
            eff->effect_expression(), result, sub_from, sub_to);
        result = z3::ite(sub_cond, with_effect, result);
    }

    return result;
}

z3::expr CausalLazyR2EPlanner::create_effect_value_z3(
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
// Achiever disjunctions
// ---------------------------------------------------------------------------

void CausalLazyR2EPlanner::add_precondition_achiever_disjunctions(size_t slot_idx) {
    // For each precondition of the action at slot_idx, add a redundant clause:
    //   act_s => ∨{¬blk_sM : M < slot_idx, action at sM can achieve precond}
    //
    // Using ¬blk instead of act_var avoids the issue where blocking assumptions
    // transitively kill activated slots.  When a preceding achiever is activated
    // (blk not assumed), ¬blk can be true, satisfying the disjunction.  When all
    // preceding achievers are blocked, act_s is forced false — correct pruning
    // since the precondition genuinely can't be achieved.
    const ActionSlot& slot = chain_[slot_idx];
    auto prec_it = action_precondition_ids_.find(slot.action);
    if (prec_it == action_precondition_ids_.end()) return;

    for (ExprID prec_cond : prec_it->second) {
        if (init_satisfied_conditions_.count(prec_cond)) continue;

        auto ach_it = condition_achievers_.find(prec_cond);
        if (ach_it == condition_achievers_.end()) continue;

        const auto& achiever_actions = ach_it->second;
        std::unordered_set<const Action*> achiever_set(
            achiever_actions.begin(), achiever_actions.end());

        // Build disjunction of ¬blk for preceding achiever slots
        z3::expr_vector disjuncts(ctx_);
        for (size_t i = 0; i < slot_idx; ++i) {
            if (achiever_set.count(chain_[i].action)) {
                disjuncts.push_back(!chain_[i].blocking_lit);
            }
        }

        if (disjuncts.empty()) continue;  // No preceding achievers yet

        // act_s => ∨{¬blk_preceding_achievers}
        z3::expr prec_clause = z3::implies(slot.action_var, z3::mk_or(disjuncts));
        solver_.add(prec_clause);
        if (Config::instance().is_debug()) {
            Logger::instance().info("  PREC_ACH: " + prec_clause.to_string());
        }
    }
}

void CausalLazyR2EPlanner::refresh_goal_achiever_disjunctions() {
    // For each unsatisfied goal condition, add a hard clause tied to each
    // active goal_lit:
    //   goal_lit => ∨{¬blk_s : slot s has an achiever of this condition}
    //
    // Using ¬blk instead of act_var ensures the conflict is detected by
    // pure boolean unit propagation.  Tying the clause to goal_lit (instead
    // of a separate assumption) means the boolean conflict competes with
    // the theory-level proof on the SAME watch list.  When all achievers
    // are blocked, BCP finds the conflict during the watch list scan of
    // ¬goal_lit, producing core = {goal_lit, blk_a1, ..., blk_ak}.

    for (size_t ci = 0; ci < goal_condition_ids_.size(); ++ci) {
        ExprID cond = goal_condition_ids_[ci];
        if (init_satisfied_conditions_.count(cond)) continue;

        auto ach_it = condition_achievers_.find(cond);
        if (ach_it == condition_achievers_.end()) continue;

        const auto& achiever_actions = ach_it->second;
        std::unordered_set<const Action*> achiever_set(
            achiever_actions.begin(), achiever_actions.end());

        // Build disjunction of ¬blk for all chain slots with achiever actions
        z3::expr_vector disjuncts(ctx_);
        for (const auto& slot : chain_) {
            if (achiever_set.count(slot.action)) {
                disjuncts.push_back(!slot.blocking_lit);
            }
        }

        if (disjuncts.empty()) continue;

        z3::expr disj = z3::mk_or(disjuncts);

        // Tie to each active goal_lit — redundant clause, provides short proof
        for (const auto& ga : goal_assumptions_) {
            if (ga.active) {
                z3::expr clause = z3::implies(ga.lit, disj);
                solver_.add(clause);
                if (Config::instance().is_debug()) {
                    Logger::instance().info("  CLAUSE: " + clause.to_string());
                }
            }
        }

        if (Config::instance().is_verbose() || Config::instance().is_debug()) {
            Logger::instance().info("  Goal ach disj [" + std::to_string(ci) +
                "]: " + std::to_string(disjuncts.size()) + " achiever slots");
        }
    }
}

// ---------------------------------------------------------------------------
// Goal management
// ---------------------------------------------------------------------------

void CausalLazyR2EPlanner::setup_goal_assumptions() {
    refresh_goal_assumptions();
}

void CausalLazyR2EPlanner::refresh_goal_assumptions() {
    for (auto& ga : goal_assumptions_) ga.active = false;

    int version = next_goal_version_++;

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

    // Also refresh achiever disjunctions
    refresh_goal_achiever_disjunctions();
}

// ---------------------------------------------------------------------------
// Search loop helpers
// ---------------------------------------------------------------------------

z3::expr_vector CausalLazyR2EPlanner::build_assumptions() {
    z3::expr_vector assumptions(ctx_);

    // 1. Blocking literals — these set act_var = false for blocked slots
    for (const auto& slot : chain_) {
        if (!slot.activated) {
            assumptions.push_back(slot.blocking_lit);
        }
    }

    // 2. Goal literals — achiever disjunctions are tied to these same lits
    //    as hard clauses (goal_lit => ∨{¬blk_achievers}).  When goal_lit is
    //    pushed and all achievers are blocked, the boolean conflict produces
    //    core = {goal_lit, blk_a1, ..., blk_ak}.
    for (const auto& ga : goal_assumptions_) {
        if (ga.active) {
            assumptions.push_back(ga.lit);
        }
    }

    return assumptions;
}

int CausalLazyR2EPlanner::process_core(const z3::expr_vector& core) {
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

    std::sort(to_replenish.begin(), to_replenish.end(),
        [this](const Action* a, const Action* b) {
            return action_rank_.at(a) < action_rank_.at(b);
        });

    for (const Action* action : to_replenish) {
        append_slot(action, /*blocked=*/true);
    }

    if (activated > 0) {
        refresh_goal_assumptions();
    }

    return activated;
}

int CausalLazyR2EPlanner::activate_goal_achievers() {
    // When the solver's core contains only goal literals (no blocking literals),
    // the achiever disjunctions were not used in the proof because the direct
    // goal falsification (goal_lit => G[init] = false) is a shorter proof path.
    // We manually apply the achiever logic: activate blocked slots whose actions
    // are achievers of unsatisfied goal conditions.
    int activated = 0;
    std::vector<const Action*> to_replenish;

    for (ExprID cond : goal_condition_ids_) {
        if (init_satisfied_conditions_.count(cond)) continue;

        auto ach_it = condition_achievers_.find(cond);
        if (ach_it == condition_achievers_.end()) continue;

        std::unordered_set<const Action*> achiever_set(
            ach_it->second.begin(), ach_it->second.end());

        for (size_t i = 0; i < chain_.size(); ++i) {
            if (!chain_[i].activated && achiever_set.count(chain_[i].action)) {
                chain_[i].activated = true;
                block_id_to_chain_index_.erase(chain_[i].blocking_lit.id());
                to_replenish.push_back(chain_[i].action);
                activated++;
            }
        }
    }

    std::sort(to_replenish.begin(), to_replenish.end(),
        [this](const Action* a, const Action* b) {
            return action_rank_.at(a) < action_rank_.at(b);
        });

    for (const Action* action : to_replenish) {
        append_slot(action, /*blocked=*/true);
    }

    if (activated > 0) {
        refresh_goal_assumptions();
    }

    return activated;
}

void CausalLazyR2EPlanner::extend_chain() {
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

Plan CausalLazyR2EPlanner::extract_plan(const z3::model& model) {
    Plan plan;
    for (const auto& slot : chain_) {
        if (!slot.activated) continue;
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

Plan CausalLazyR2EPlanner::search() {
    auto& config = Config::instance();
    auto& stats = Stats::instance();

    solution_found_ = false;
    timed_out_ = false;
    init_deadline();

    Logger::instance().info("Starting Causal Lazy R2E search, timeout: " + format_timeout_string());

    // 1. Encode initial state at timestep 0
    solver_.add(*encoder_.encode_initial_state());

    // 2. Initialize chain tails to timestep-0 variables for all modifiable fluents
    for (ExprID fid : modifiable_fluents_) {
        z3::expr base_var = encoder_.convert_expr_id_to_z3(fid, 0);
        var_info_.emplace(fid, VarChainInfo{base_var});
    }

    // 3. Compute action ordering once (ARPG-based)
    compute_action_ordering();

    // 4. Initialize achiever analysis (ARPG + SMT-based)
    initialize_achievers();

    // 5. Build initial frontier: one blocked slot per action (ARPG order)
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

    // 6. Setup goal assumptions (also sets up goal achiever disjunctions)
    setup_goal_assumptions();

    // 7. Search loop
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
        });

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
                for (unsigned ci = 0; ci < core.size() && ci < 20; ++ci) {
                    Logger::instance().info("    core[" + std::to_string(ci) + "]: " +
                        core[ci].to_string());
                }
            }

            if (activated == 0) {
                // The solver's core contains only goal/achiever literals.
                // Try achiever-guided activation once; if the solver still
                // can't make progress after that, extend the chain to add
                // new capacity (as the base LazyR2E planner does).
                if (!achiever_activation_pending_) {
                    activated = activate_goal_achievers();
                    if (activated > 0) {
                        achiever_activation_pending_ = true;
                        if (config.is_verbose() || config.is_debug()) {
                            Logger::instance().info("  Achiever-guided activation: " +
                                std::to_string(activated));
                        }
                    }
                }

                if (activated == 0) {
                    // Either achiever activation was already tried, or no
                    // achiever slots available.  Extend the chain.
                    achiever_activation_pending_ = false;
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
                // Solver-driven activation succeeded — reset the flag.
                achiever_activation_pending_ = false;
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
