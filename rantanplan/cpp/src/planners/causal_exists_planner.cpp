#include "causal_exists_planner.hpp"
#include "../config/config.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include "../util/memory_tracker.hpp"
#include <algorithm>
#include <chrono>

namespace rantanplan {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CausalExistsPlanner::CausalExistsPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
    : BasePlanner(problem, encoder, ctx),
      goal_assumption_(ctx.bool_val(true)) {
}

// ---------------------------------------------------------------------------
// Achiever setup (same as CausalLazyR2EPlanner)
// ---------------------------------------------------------------------------

void CausalExistsPlanner::build_action_id_map() {
    for (const Action& a : problem_.actions()) {
        action_id_to_ptr_[a.id()] = &a;
    }
}

void CausalExistsPlanner::build_condition_achiever_cache() {
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

    for (ExprID cond : achievers_->get_goal_conditions()) {
        goal_condition_ids_.push_back(cond);
    }

    for (const Action& a : problem_.actions()) {
        const auto& precs = achievers_->get_preconditions(a);
        if (!precs.empty()) {
            auto* ptr = action_id_to_ptr_[a.id()];
            auto& vec = action_precondition_ids_[ptr];
            vec.assign(precs.begin(), precs.end());
        }
    }
}

void CausalExistsPlanner::compute_init_satisfied_conditions() {
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

void CausalExistsPlanner::initialize_achievers() {
    auto init_start = std::chrono::high_resolution_clock::now();

    build_action_id_map();
    achievers_ = std::make_unique<AchieversAnalysis>(problem_);
    build_condition_achiever_cache();
    compute_init_satisfied_conditions();

    auto init_end = std::chrono::high_resolution_clock::now();
    double init_time = std::chrono::duration<double>(init_end - init_start).count();

    Logger::instance().info("Causal Exists achiever analysis: " +
        std::to_string(condition_achievers_.size()) + " conditions, " +
        std::to_string(init_satisfied_conditions_.size()) + " init-satisfied, " +
        std::to_string(goal_condition_ids_.size()) + " goal conditions " +
        "(" + std::to_string(init_time) + "s)");

    // Direct goal achievers
    for (ExprID cond : goal_condition_ids_) {
        if (init_satisfied_conditions_.count(cond)) continue;
        auto it = condition_achievers_.find(cond);
        if (it != condition_achievers_.end()) {
            for (const Action* a : it->second) {
                goal_achiever_actions_.insert(a);
            }
        }
    }

    // Transitive achiever closure via BFS
    {
        std::vector<ExprID> frontier;
        std::unordered_set<ExprID> visited;
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
                " -> achievers: [" + achievers_str + "]");
        }
    }
}

// ---------------------------------------------------------------------------
// Timestep management
// ---------------------------------------------------------------------------

void CausalExistsPlanner::add_timestep(int t) {
    // Encode standard exists-step constraints at this timestep
    add_timestep_constraints(solver_, encoder_, *propagator_strategy_, t,
                             /*prefix_monotone=*/false);

    // Add blocking constraints: block_a_t -> not act_a_t
    auto& vf = encoder_.get_variable_factory();
    for (const Action& action : problem_.actions()) {
        const z3::expr& action_var = vf.get_action_variable(action, t);

        std::string block_name = "blk_a" + std::to_string(action.id()) + "_t" + std::to_string(t);
        z3::expr block_lit = ctx_.bool_const(block_name.c_str());

        solver_.add(z3::implies(block_lit, !action_var));

        size_t idx = block_entries_.size();
        block_entries_.push_back({block_lit, &action, t, true});
        block_id_to_index_[block_lit.id()] = idx;
        blocked_count_[&action]++;
    }
}

void CausalExistsPlanner::refresh_goal(int t) {
    goal_timestep_ = t;
    int version = next_goal_version_++;
    std::string lit_name = "goal_v" + std::to_string(version);
    goal_assumption_ = ctx_.bool_const(lit_name.c_str());
    solver_.add(z3::implies(goal_assumption_, *encoder_.encode_goal(t)));
}

void CausalExistsPlanner::extend_horizon() {
    current_horizon_++;
    add_timestep(current_horizon_);
    // Goal at the last state: transition t produces state t+1
    refresh_goal(current_horizon_ + 1);

    int pred_count = predictive_activate(current_horizon_);

    if (Config::instance().is_verbose() && pred_count > 0) {
        Logger::instance().info("  Predictive: activated " +
            std::to_string(pred_count) + " actions at t=" +
            std::to_string(current_horizon_));
    }
}

// ---------------------------------------------------------------------------
// Search loop helpers
// ---------------------------------------------------------------------------

z3::expr_vector CausalExistsPlanner::build_assumptions() {
    z3::expr_vector assumptions(ctx_);

    // All active blocking literals
    for (const auto& entry : block_entries_) {
        if (entry.active) {
            assumptions.push_back(entry.lit);
        }
    }

    // Goal assumption
    assumptions.push_back(goal_assumption_);

    return assumptions;
}

int CausalExistsPlanner::process_core(const z3::expr_vector& core) {
    int activated = 0;
    int filtered = 0;
    std::vector<const Action*> bumped_actions;

    for (unsigned i = 0; i < core.size(); ++i) {
        unsigned eid = core[i].id();
        auto it = block_id_to_index_.find(eid);
        if (it != block_id_to_index_.end()) {
            size_t idx = it->second;
            BlockEntry& entry = block_entries_[idx];
            const Action* action = entry.action;

            // Filter: skip non-goal-relevant actions
            if (!goal_relevant_actions_.count(action)) {
                filtered++;
                continue;
            }

            // Activate: deactivate the blocking entry
            entry.active = false;
            block_id_to_index_.erase(it);
            blocked_count_[action]--;
            activity_[action] += 1.0;
            bumped_actions.push_back(action);
            activated++;
        }
    }

    if (Config::instance().is_verbose() && filtered > 0) {
        Logger::instance().info("  Filtered: " + std::to_string(filtered) +
            " non-achiever blocking lits from core");
    }

    // Cascade bump enablers
    cascade_bump(bumped_actions, 1.0);

    // Check replenishment invariant: if any activated action has no
    // remaining blocked timestep, we need to extend the horizon
    bool need_extension = false;
    for (const Action* action : bumped_actions) {
        if (blocked_count_[action] <= 0) {
            need_extension = true;
            break;
        }
    }
    if (need_extension) {
        extend_horizon();
    }

    return activated;
}

void CausalExistsPlanner::cascade_bump(
    const std::vector<const Action*>& seeds, double bump_amount) {

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
            activity_[achiever] += capped;
            frontier.push_back(achiever);

            if (Config::instance().is_debug()) {
                Logger::instance().info("    Bump: " + achiever->name() +
                    " += " + std::to_string(capped) +
                    " -> a=" + std::to_string(activity_[achiever]));
            }
        }
    }
}

int CausalExistsPlanner::predictive_activate(int timestep) {
    int activated = 0;

    for (size_t i = 0; i < block_entries_.size(); ++i) {
        BlockEntry& entry = block_entries_[i];
        if (!entry.active || entry.timestep != timestep) continue;

        double score = activity_[entry.action];
        if (score <= activation_threshold_) continue;

        entry.active = false;
        block_id_to_index_.erase(entry.lit.id());
        blocked_count_[entry.action]--;
        activated++;
    }

    return activated;
}

void CausalExistsPlanner::decay_activity() {
    for (auto& [action, score] : activity_) {
        score *= activity_decay_;
    }
}

Plan CausalExistsPlanner::extract_plan(const z3::model& model) {
    // extract_plan uses exclusive upper bound: for (t = 0; t < max_timestep; ++t)
    // We encoded transitions [0, current_horizon_], so pass current_horizon_ + 1
    return encoder_.extract_plan(model, current_horizon_ + 1);
}

// ---------------------------------------------------------------------------
// Main search
// ---------------------------------------------------------------------------

Plan CausalExistsPlanner::search() {
    auto& config = Config::instance();
    auto& stats = Stats::instance();

    solution_found_ = false;
    timed_out_ = false;
    init_deadline();

    Logger::instance().info("Starting Causal Exists search, timeout: " + format_timeout_string());

    // 1. Encode initial state
    solver_.add(*encoder_.encode_initial_state());
    propagator_strategy_->register_timestep_variables(0);

    // 2. Initialize achiever analysis
    initialize_achievers();

    // 3. First horizon: timestep 0 with all actions blocked
    extend_horizon();

    // 4. Seed: activate goal achievers at timestep 0
    {
        int seeded = 0;
        for (size_t i = 0; i < block_entries_.size(); ++i) {
            BlockEntry& entry = block_entries_[i];
            if (!entry.active || entry.timestep != 0) continue;
            if (!goal_achiever_actions_.count(entry.action)) continue;

            entry.active = false;
            block_id_to_index_.erase(entry.lit.id());
            blocked_count_[entry.action]--;
            activity_[entry.action] = 1.0;
            seeded++;
        }

        // Cascade bump from goal achievers and predictive-activate enablers
        std::vector<const Action*> seeds(goal_achiever_actions_.begin(),
                                         goal_achiever_actions_.end());
        cascade_bump(seeds, 1.0);
        int pred_count = predictive_activate(0);

        Logger::instance().info("Initial: " + std::to_string(seeded) +
            " goal achievers seeded + " + std::to_string(pred_count) +
            " cascade enablers at t=0, " +
            std::to_string(problem_.actions().size()) + " actions");
    }

    // 5. Check replenishment after seeding
    {
        bool need_ext = false;
        for (const Action& action : problem_.actions()) {
            if (blocked_count_[&action] <= 0) {
                need_ext = true;
                break;
            }
        }
        if (need_ext) {
            extend_horizon();
        }
    }

    // 6. Search loop
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

        // Count active blocking entries
        size_t active_blocked = 0;
        for (const auto& e : block_entries_) if (e.active) active_blocked++;

        double mem = MemoryTracker::instance().get_current_memory_mb();

        Logger::instance().timestep_solving(VerbosityLevel::INFO, round, {
            {"solve", std::to_string(solve_time) + "s"},
            {"round", std::to_string(round_time) + "s"},
            {"horizon", std::to_string(current_horizon_)},
            {"blocked", std::to_string(active_blocked)},
            {"total_entries", std::to_string(block_entries_.size())},
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
                "horizon=" + std::to_string(current_horizon_) +
                " (total time: " + std::to_string(total_time) + "s) ***");

            stats.set("planner.plan_length", static_cast<double>(plan.length()));
            stats.set("planner.rounds", static_cast<double>(round));
            stats.set("planner.solution_horizon", static_cast<double>(current_horizon_));

            plan.write_ipc(config.planner.output_plan, 1,
                           -1.0, true,
                           config.planner.strategy, config.planner.mode, total_time);

            collect_statistics();
            propagator_strategy_->cleanup();
            return plan;

        } else if (result == z3::unsat) {
            z3::expr_vector core = solver_.unsat_core();

            if (config.is_verbose() || config.is_debug()) {
                int core_blocking = 0, core_goals = 0;
                for (unsigned ci = 0; ci < core.size(); ++ci) {
                    unsigned eid = core[ci].id();
                    if (block_id_to_index_.find(eid) != block_id_to_index_.end()) {
                        core_blocking++;
                    } else {
                        core_goals++;
                    }
                }
                Logger::instance().info("  Core size: " + std::to_string(core.size()) +
                    " (blocking=" + std::to_string(core_blocking) +
                    ", goals=" + std::to_string(core_goals) + ")");
            }

            int activated = process_core(core);

            if (config.is_verbose()) {
                Logger::instance().info("  Activated: " + std::to_string(activated));
            }

            if (activated == 0) {
                if (current_horizon_ >= config.planner.max_steps) {
                    Logger::instance().info("Horizon limit reached (" +
                        std::to_string(current_horizon_) + ")");
                    break;
                }
                extensions++;
                Logger::instance().info("Extending horizon (extension #" +
                    std::to_string(extensions) + ", horizon: " +
                    std::to_string(current_horizon_) + ")");
                extend_horizon();
            }

        } else {
            if (handle_unknown_result(solver_, "round " + std::to_string(round))) break;
        }

        decay_activity();
        round++;
    }

    if (timed_out_) {
        Logger::instance().info("No plan found (timeout).");
    } else {
        Logger::instance().info("\n*** NO PLAN FOUND within horizon limit ***");
    }

    collect_statistics();
    propagator_strategy_->cleanup();
    return Plan();
}

} // namespace rantanplan
