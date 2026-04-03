#include "causal_lazy_r2e_planner.hpp"
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
    NumericRelaxedPlanningGraph rpg(problem_);
    if (rpg.build()) {
        auto ordered = rpg.get_action_ordering();
        if (!ordered.empty()) {
            Logger::instance().info("Causal Lazy R2E: using RPG ordering (" +
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

    // Build the set of actions that are (direct) goal achievers
    for (ExprID cond : goal_condition_ids_) {
        if (init_satisfied_conditions_.count(cond)) continue;
        auto it = condition_achievers_.find(cond);
        if (it != condition_achievers_.end()) {
            for (const Action* a : it->second) {
                goal_achiever_actions_.insert(a);
            }
        }
    }

    // Build transitive achiever set: BFS from all goal conditions through
    // the achiever graph.  Any action reachable is "goal-relevant" — its
    // blocking literal in an UNSAT core is meaningful.  Actions NOT in
    // this set (e.g. self-loop flights) are noise and will be filtered.
    {
        std::vector<ExprID> frontier;
        std::unordered_set<ExprID> visited;
        // Seed with all goal conditions (don't skip init-satisfied ones:
        // a condition true initially may need re-achievement after being
        // destroyed, e.g. located(plane1,city0) after flying away).
        for (ExprID cond : goal_condition_ids_) {
            frontier.push_back(cond);
            visited.insert(cond);
        }

        while (!frontier.empty()) {
            std::vector<ExprID> next_frontier;
            for (ExprID cond : frontier) {
                auto ach_it = condition_achievers_.find(cond);
                if (ach_it == condition_achievers_.end()) continue;
                for (const Action* achiever : ach_it->second) {
                    if (goal_relevant_actions_.insert(achiever).second) {
                        auto prec_it = action_precondition_ids_.find(achiever);
                        if (prec_it != action_precondition_ids_.end()) {
                            for (ExprID prec_cond : prec_it->second) {
                                if (visited.insert(prec_cond).second) {
                                    next_frontier.push_back(prec_cond);
                                }
                            }
                        }
                    }
                }
            }
            frontier = std::move(next_frontier);
        }

        Logger::instance().info("  Goal-relevant actions: " +
            std::to_string(goal_relevant_actions_.size()) + "/" +
            std::to_string(problem_.actions().size()));
    }

    // Debug: print goal condition → achiever mapping
    if (Config::instance().is_verbose()) {
        for (ExprID cond : goal_condition_ids_) {
            std::string cond_str = problem_.pool().to_string(cond);
            auto it = condition_achievers_.find(cond);
            std::string achievers_str;
            if (it != condition_achievers_.end()) {
                for (const Action* a : it->second) {
                    if (!achievers_str.empty()) achievers_str += ", ";
                    achievers_str += a->name() + "(id=" + std::to_string(a->id()) + ")";
                }
            }
            bool init_sat = init_satisfied_conditions_.count(cond) > 0;
            Logger::instance().info("  Goal cond: " + cond_str +
                (init_sat ? " [INIT]" : "") +
                " → achievers: [" + achievers_str + "]");
        }
    }
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
        goal_assumptions_.push_back({goal_lit, true, g.goal_id()});
    }

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

    // 2. Goal literals
    for (const auto& ga : goal_assumptions_) {
        if (ga.active) {
            assumptions.push_back(ga.lit);
        }
    }

    return assumptions;
}

int CausalLazyR2EPlanner::process_core(const z3::expr_vector& core) {
    int activated = 0;

    // Phase 1: Activate slots from the core, but only if the action
    // is a goal-relevant achiever.  Non-achiever blocking lits (e.g.
    // self-loop flights) are noise from Z3's proof search.
    int filtered = 0;
    std::vector<const Action*> bumped_actions;
    std::vector<const Action*> phase1_replenish;
    for (unsigned i = 0; i < core.size(); ++i) {
        unsigned eid = core[i].id();
        auto it = block_id_to_chain_index_.find(eid);
        if (it != block_id_to_chain_index_.end()) {
            size_t idx = it->second;
            const Action* action = chain_[idx].action;

            // Filter: skip actions that aren't transitive goal achievers
            if (!goal_relevant_actions_.count(action)) {
                filtered++;
                continue;
            }

            chain_[idx].activated = true;
            block_id_to_chain_index_.erase(it);
            phase1_replenish.push_back(action);
            active_count_[action]++;
            multiplicity_[action] += 1.0;
            bumped_actions.push_back(action);
            activated++;
        }
    }

    if (Config::instance().is_verbose() && filtered > 0) {
        Logger::instance().info("  Filtered: " + std::to_string(filtered) +
            " non-achiever blocking lits from core");
    }

    // Phase 2: Cascade bump enablers via BFS through achiever graph.
    cascade_bump(bumped_actions, 1.0);

    // Phase 3: Threshold activation
    int threshold_count = threshold_activate();
    activated += threshold_count;

    if (Config::instance().is_verbose() && threshold_count > 0) {
        Logger::instance().info("  Threshold: activated " +
            std::to_string(threshold_count) + " enablers");
    }

    // Replenish Phase 1 activations
    std::sort(phase1_replenish.begin(), phase1_replenish.end(),
        [this](const Action* a, const Action* b) {
            return action_rank_.at(a) < action_rank_.at(b);
        });
    for (const Action* action : phase1_replenish) {
        append_slot(action, /*blocked=*/true);
    }

    if (activated > 0) {
        refresh_goal_assumptions();
    }

    return activated;
}

void CausalLazyR2EPlanner::cascade_bump(
    const std::vector<const Action*>& seeds, double bump_amount) {

    // BFS through the achiever graph with NO fixed per-layer decay.
    // Fan-out normalization at each step acts as adaptive decay:
    // narrow fan-out (drone: 2 achievers) → 0.5 per achiever;
    // wide fan-out (zenotravel: 20 fly variants) → 0.05 per achiever.
    //
    // A visited set prevents infinite loops on cycles, and a per-action
    // cap prevents runaway amplification from many parents.
    //
    // Global VSIDS-style decay (applied once per round in the search
    // loop, not here) ensures that old cascade evidence fades — only
    // actions with recent/repeated support maintain high scores.

    std::unordered_set<const Action*> visited(seeds.begin(), seeds.end());
    std::vector<const Action*> frontier = seeds;

    while (!frontier.empty()) {
        std::unordered_map<const Action*, double> next_bumps;

        for (const Action* action : frontier) {
            auto prec_it = action_precondition_ids_.find(action);
            if (prec_it == action_precondition_ids_.end()) continue;

            for (ExprID prec_cond : prec_it->second) {
                if (init_satisfied_conditions_.count(prec_cond)) continue;

                auto ach_it = condition_achievers_.find(prec_cond);
                if (ach_it == condition_achievers_.end()) continue;

                double per_achiever = bump_amount /
                    static_cast<double>(ach_it->second.size());
                for (const Action* achiever : ach_it->second) {
                    if (!visited.count(achiever)) {
                        next_bumps[achiever] += per_achiever;
                    }
                }
            }
        }

        const double max_cascade_bump = 1.0;
        frontier.clear();
        for (auto& [achiever, bump] : next_bumps) {
            visited.insert(achiever);
            double capped = std::min(bump, max_cascade_bump);
            multiplicity_[achiever] += capped;
            frontier.push_back(achiever);

            if (Config::instance().is_debug()) {
                Logger::instance().info("    Bump: " + achiever->name() +
                    " += " + std::to_string(capped) +
                    " → μ=" + std::to_string(multiplicity_[achiever]));
            }
        }
    }
}

int CausalLazyR2EPlanner::threshold_activate() {
    int activated = 0;
    bool progress = true;

    while (progress) {
        progress = false;
        for (auto& [action, mu] : multiplicity_) {
            int target = static_cast<int>(mu);
            int current = active_count_[action];
            if (current >= target) continue;

            // Find a blocked slot for this action and activate it
            for (size_t i = 0; i < chain_.size(); ++i) {
                if (chain_[i].action == action && !chain_[i].activated) {
                    chain_[i].activated = true;
                    block_id_to_chain_index_.erase(chain_[i].blocking_lit.id());
                    active_count_[action]++;
                    activated++;
                    progress = true;

                    if (Config::instance().is_verbose()) {
                        Logger::instance().info("    Threshold: " +
                            action->name() + "_s" +
                            std::to_string(chain_[i].slot_id) +
                            " (μ=" + std::to_string(mu) +
                            ", active=" + std::to_string(active_count_[action]) + ")");
                    }

                    // Inline replenish: create replacement blocked slot
                    // so the next iteration can activate another if needed
                    append_slot(action, /*blocked=*/true);
                    break;
                }
            }
        }
    }

    return activated;
}

void CausalLazyR2EPlanner::decay_multiplicity() {
    // VSIDS-style global decay: multiply ALL multiplicity scores by
    // the decay factor.  Recent bumps dominate; old evidence fades.
    // active_count is NOT decayed — it tracks physical reality (how
    // many slots are active).  The decay only affects the "demand"
    // signal, not the supply.
    //
    // Critical invariant: after decay, μ may drop below active_count.
    // This is correct — it means the old evidence no longer justifies
    // the current supply level.  No slots are deactivated; the effect
    // is simply that threshold_activate won't fire until fresh evidence
    // pushes μ above active_count again.
    for (auto& [action, mu] : multiplicity_) {
        mu *= vsids_decay_;
    }
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

    // 5. Build initial frontier: one slot per action (ARPG order).
    //    Goal achievers start activated (μ=1); all others start blocked (μ=0).
    int actions_with_effects = 0;
    int initial_active = 0;
    for (const Action* action : action_ordering_) {
        if (!action->effects().empty()) {
            bool is_goal_achiever = goal_achiever_actions_.count(action) > 0;
            append_slot(action, /*blocked=*/!is_goal_achiever);
            actions_with_effects++;
            if (is_goal_achiever) {
                multiplicity_[action] = 1.0;
                active_count_[action] = 1;
                initial_active++;
            }
        }
    }

    // Cascade-bump enablers of goal achievers and threshold-activate.
    // Multiple goal achievers sharing the same enabler aggregate their
    // bumps, so heavily-needed enablers (e.g. increase_y for 8 visit
    // variants) cross the integer threshold and get multiple copies.
    {
        std::vector<const Action*> seeds(goal_achiever_actions_.begin(),
                                         goal_achiever_actions_.end());
        cascade_bump(seeds, 1.0);
        int cascade_count = threshold_activate();

        Logger::instance().info("Initial frontier: " + std::to_string(actions_with_effects) +
            " action slots (" + std::to_string(initial_active) + " goal achievers active + " +
            std::to_string(cascade_count) + " cascade enablers), " +
            std::to_string(var_info_.size()) + " tracked variables, " +
            std::to_string(next_slot_id_) + " chain slots");
    }

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

            // Log core BEFORE processing (so blocking lits are still in the map)
            if (config.is_verbose() || config.is_debug()) {
                // Helper: IPC-style action name "fly-slow(plane1, city0, city2)"
                auto action_label = [](const Action* a) -> std::string {
                    std::string s = a->name();
                    if (!a->parameters().empty()) {
                        s += "(";
                        for (size_t i = 0; i < a->parameters().size(); ++i) {
                            if (i > 0) s += ", ";
                            s += a->parameters()[i].name();
                        }
                        s += ")";
                    }
                    return s;
                };

                // Classify core elements
                int core_blocking = 0, core_goals = 0;
                for (unsigned ci = 0; ci < core.size(); ++ci) {
                    unsigned eid = core[ci].id();
                    if (block_id_to_chain_index_.find(eid) != block_id_to_chain_index_.end()) {
                        core_blocking++;
                    } else {
                        core_goals++;
                    }
                }

                Logger::instance().info("  Core size: " + std::to_string(core.size()) +
                    " (blocking=" + std::to_string(core_blocking) +
                    ", goals=" + std::to_string(core_goals) + ")");

                // Show first few elements in debug mode
                if (config.is_debug()) {
                    for (unsigned ci = 0; ci < core.size() && ci < 10; ++ci) {
                        std::string label = core[ci].to_string();
                        unsigned eid = core[ci].id();
                        auto bit = block_id_to_chain_index_.find(eid);
                        if (bit != block_id_to_chain_index_.end()) {
                            label += " [" + action_label(chain_[bit->second].action) + "]";
                        }
                        Logger::instance().info("    core[" + std::to_string(ci) + "]: " + label);
                    }
                    if (core.size() > 10) {
                        Logger::instance().info("    ... (" + std::to_string(core.size() - 10) + " more)");
                    }
                }
            }

            int activated = process_core(core);

            if (config.is_verbose()) {
                Logger::instance().info("  Activated: " + std::to_string(activated));
            }

            if (activated == 0) {
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

        // VSIDS-style global decay: age all multiplicity scores so
        // old cascade evidence fades and recent evidence dominates.
        decay_multiplicity();

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
