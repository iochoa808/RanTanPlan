#include "causal_exists_planner.hpp"
#include "../encoders/grounded_encoder.hpp"
#include "../config/config.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include "../util/memory_tracker.hpp"
#include <algorithm>
#include <chrono>

namespace rantanplan {

// ===========================================================================
// CausalExistsPlanner — Core-guided lazy activation for exists-step planning
//
// Architecture overview:
//   - Each (action, timestep) pair has a blocking literal: blk → ¬act
//   - Blocking literals are passed as solver assumptions; UNSAT cores
//     reveal which actions the solver needs.
//   - Two-phase lazy encoding: timestep creation adds only the eager
//     skeleton (action variables, frame axioms, symmetries); individual
//     action precondition/effect constraints are deferred until the
//     corresponding blocking literal is removed (see ensure_action_encoded).
//   - Parallelism (exists-step acyclicity) is enforced by the Z3 user
//     propagator, not by explicit mutex clauses.
//   - The replenishment invariant guarantees every action always has at
//     least one blocked entry, ensuring the solver can always signal the
//     need for an action via an UNSAT core.
// ===========================================================================

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CausalExistsPlanner::CausalExistsPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
    : BasePlanner(problem, encoder, ctx),
      goal_assumption_(ctx.bool_val(true)) {
}

GroundedEncoder& CausalExistsPlanner::grounded_encoder() {
    if (!grounded_encoder_) {
        grounded_encoder_ = &dynamic_cast<GroundedEncoder&>(encoder_);
    }
    return *grounded_encoder_;
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

    auto init_end = std::chrono::high_resolution_clock::now();
    double init_time = std::chrono::duration<double>(init_end - init_start).count();

    Logger::instance().info("Causal Exists achiever analysis: " +
        std::to_string(condition_achievers_.size()) + " conditions, " +
        std::to_string(init_satisfied_conditions_.size()) + " init-satisfied, " +
        std::to_string(goal_condition_ids_.size()) + " goal conditions " +
        "(" + std::to_string(init_time) + "s)");

    // Direct goal achievers
    for (ExprID g : goal_condition_ids_) {
        auto it = condition_achievers_.find(g);
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

    // Extract relaxed plan (h^ff-style)
    extract_relaxed_plan();
}

void CausalExistsPlanner::extract_relaxed_plan() {
    // h^ff-style backward extraction from ARPG layers.
    // Starting from goal conditions not satisfied in the initial state,
    // greedily selects the earliest-layer achiever for each unsatisfied
    // condition, adds that achiever's unsatisfied preconditions to the
    // open set, and iterates backward through layers. The resulting
    // relaxed plan provides a heuristic estimate of useful actions; its
    // length is logged alongside ARPG metrics to calibrate search effort.
    relaxed_plan_.clear();

    std::unordered_set<ExprID> unsatisfied;
    for (ExprID g : goal_condition_ids_) {
        if (!init_satisfied_conditions_.count(g)) {
            unsatisfied.insert(g);
        }
    }

    std::unordered_set<const Action*> used;
    int max_layer = achievers_->get_arpg_num_layers();

    for (int layer = max_layer - 1; layer >= 0 && !unsatisfied.empty(); layer--) {
        std::vector<ExprID> newly_satisfied;

        for (ExprID cond : unsatisfied) {
            auto it = condition_achievers_.find(cond);
            if (it == condition_achievers_.end()) continue;

            const Action* best = nullptr;
            int best_layer = -1;
            for (const Action* a : it->second) {
                int a_layer = achievers_->get_action_first_layer(a->id());
                if (a_layer <= layer && a_layer > best_layer) {
                    best = a;
                    best_layer = a_layer;
                }
            }

            if (best && !used.count(best)) {
                used.insert(best);
                relaxed_plan_.push_back(best);
                newly_satisfied.push_back(cond);

                auto prec_it = action_precondition_ids_.find(best);
                if (prec_it != action_precondition_ids_.end()) {
                    for (ExprID prec : prec_it->second) {
                        if (!init_satisfied_conditions_.count(prec)) {
                            unsatisfied.insert(prec);
                        }
                    }
                }
            }
        }

        for (ExprID cond : newly_satisfied) {
            unsatisfied.erase(cond);
        }
    }

    if (Config::instance().is_verbose()) {
        for (const Action* a : relaxed_plan_) {
            Logger::instance().info("    RP action: " + a->name() +
                " (ARPG layer=" + std::to_string(achievers_->get_action_first_layer(a->id())) + ")");
        }
    }
}

// ---------------------------------------------------------------------------
// Timestep management
// ---------------------------------------------------------------------------

void CausalExistsPlanner::add_timestep(int t) {
    // Phase 1 (eager skeleton): create action variables and encode frame
    // axioms + symmetries. Frame axioms reference all action variables in
    // modifier disjunctions, but blocked actions are forced false by
    // assumptions and cannot satisfy any disjunction. Precondition/effect
    // constraints are NOT added here — they are deferred to Phase 2
    // (ensure_action_encoded) when the blocking literal is removed.
    grounded_encoder().ensure_action_variables(t);
    solver_.add(*encoder_.encode_frames(t));
    solver_.add(*encoder_.encode_symmetries(t));
    propagator_strategy_->register_timestep_variables(t + 1);

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
    refresh_goal(current_horizon_ + 1);

    int act_count = predictive_activate(current_horizon_);

    if (Config::instance().is_verbose() && act_count > 0) {
        Logger::instance().info("  Extended horizon: activated " +
            std::to_string(act_count) + " actions at t=" +
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
    // Filter non-goal-relevant blocking literals from the core, then
    // activate the remaining ones (encode constraints + remove from
    // assumptions). Activity is bumped by 1.0 per core hit; no cascade
    // bump is applied here (cascade is only used during initial seeding).
    // Replenishment is batched: after all activations, if ANY action
    // exhausted its blocked entries, a single extend_horizon() restores
    // the invariant for all actions.
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

            // Activate: encode constraints, deactivate blocking
            deactivate_block_entry(idx);
            activity_[action] += 1.0;
            bumped_actions.push_back(action);
            activated++;
        }
    }

    if (Config::instance().is_verbose() && filtered > 0) {
        Logger::instance().info("  Filtered: " + std::to_string(filtered) +
            " non-achiever blocking lits from core");
    }

    cumulative_core_activations_ += activated;

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
    // BFS-based activity propagation through the achiever graph.
    // Used ONLY during initial seeding (not after core processing).
    // For each action in the frontier, distributes bump_amount equally
    // among achievers of its unsatisfied preconditions, capped at 1.0
    // per achiever. Propagates transitively until no new actions are found.
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
                Logger::instance().info("    Cascade bump: " + achiever->name() +
                    " += " + std::to_string(capped) +
                    " -> activity=" + std::to_string(activity_[achiever]));
            }
        }
    }
}

int CausalExistsPlanner::predictive_activate(int timestep) {
    // Heuristic: at a newly created timestep, pre-activate actions whose
    // activity score exceeds the threshold (τ=0.5). Budget is capped at
    // max(1, cumulative_core_activations) to stay conservative relative
    // to the solver's demonstrated need. Does not affect soundness or
    // completeness — only reduces the number of solver invocations.
    int budget = std::max(1, cumulative_core_activations_);

    // Collect candidates above threshold, sorted by score descending
    std::vector<std::pair<double, size_t>> candidates;
    for (size_t i = 0; i < block_entries_.size(); ++i) {
        BlockEntry& entry = block_entries_[i];
        if (!entry.active || entry.timestep != timestep) continue;
        double score = activity_[entry.action];
        if (score <= activation_threshold_) continue;
        candidates.push_back({score, i});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    int activated = 0;
    for (const auto& [score, idx] : candidates) {
        if (activated >= budget) break;

        const Action* action = deactivate_block_entry(idx);
        activated++;

        if (Config::instance().is_verbose()) {
            Logger::instance().info("    Predictive activate: " +
                action->name() + " at t=" + std::to_string(timestep) +
                " (activity=" + std::to_string(score) +
                ", blocked_remaining=" + std::to_string(blocked_count_[action]) +
                ", budget=" + std::to_string(budget) + ")");
        }
    }

    return activated;
}

void CausalExistsPlanner::decay_activity() {
    for (auto& [action, score] : activity_) {
        score *= activity_decay_;
    }
}

const Action* CausalExistsPlanner::deactivate_block_entry(size_t idx) {
    BlockEntry& entry = block_entries_[idx];
    ensure_action_encoded(entry.action, entry.timestep);
    entry.active = false;
    block_id_to_index_.erase(entry.lit.id());
    blocked_count_[entry.action]--;
    return entry.action;
}

void CausalExistsPlanner::ensure_action_encoded(const Action* action, int timestep) {
    // Phase 2 (on-demand): encode a single action's precondition/effect
    // constraints. Called by deactivate_block_entry BEFORE removing the
    // blocking assumption, maintaining the invariant that an action
    // variable can only be set to true if its constraints are present.
    auto key = std::make_pair(action->id(), timestep);
    if (action_encoded_.count(key)) return;
    action_encoded_.insert(key);

    auto constraints = grounded_encoder().encode_single_action(*action, timestep);
    if (constraints) {
        solver_.add(*constraints);
    }
}

bool CausalExistsPlanner::activate_action_at(const Action* action, int timestep) {
    for (size_t i = 0; i < block_entries_.size(); ++i) {
        BlockEntry& entry = block_entries_[i];
        if (entry.active && entry.action == action && entry.timestep == timestep) {
            deactivate_block_entry(i);
            return true;
        }
    }
    return false;
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

    // 3. Pre-extend horizon using RPG lower bound + ARPG layer ordering
    {
        int start_ts = config.planner.start_timestep;
        int num_timesteps = std::max(1, start_ts);

        for (int t = 0; t < num_timesteps; t++) {
            current_horizon_++;
            add_timestep(current_horizon_);
        }
        refresh_goal(current_horizon_ + 1);

        if (start_ts > 1) {
            Logger::instance().info("Pre-extended horizon to " +
                std::to_string(current_horizon_) +
                " (RPG lower bound: " + std::to_string(start_ts) + ")");
        }
    }

    // 4. ARPG-guided seeding: place goal achievers and their enablers
    //    at timesteps proportional to their ARPG layer.
    //    cascade_bump is used HERE and ONLY here — during the main search
    //    loop, activity is maintained purely by core hits + decay.
    {
        int arpg_layers = achievers_->get_arpg_num_layers();

        auto target_timestep = [&](const Action* a) -> int {
            if (current_horizon_ == 0 || arpg_layers <= 1) return 0;
            int layer = achievers_->get_action_first_layer(a->id());
            return std::min(current_horizon_,
                            layer * current_horizon_ / (arpg_layers - 1));
        };

        // Seed goal achievers at ARPG-proportional timesteps
        int seeded = 0;
        for (const Action* action : goal_achiever_actions_) {
            int t = target_timestep(action);
            if (activate_action_at(action, t)) {
                activity_[action] = 1.0;
                seeded++;
                if (config.is_verbose()) {
                    Logger::instance().info("  Seed goal achiever: " +
                        action->name() + " at t=" + std::to_string(t) +
                        " (ARPG layer=" +
                        std::to_string(achievers_->get_action_first_layer(action->id())) + ")");
                }
            }
        }

        // Cascade bump from goal achievers
        std::vector<const Action*> seeds(goal_achiever_actions_.begin(),
                                         goal_achiever_actions_.end());
        cascade_bump(seeds, 1.0);

        // Activate cascade-bumped enablers at ARPG-proportional timesteps.
        // Cap to top-K by score, where K = number of goal achievers seeded.
        std::vector<std::pair<double, const Action*>> enabler_candidates;
        for (auto& [action, score] : activity_) {
            if (score <= activation_threshold_) continue;
            if (goal_achiever_actions_.count(action)) continue;
            enabler_candidates.push_back({score, action});
        }
        std::sort(enabler_candidates.begin(), enabler_candidates.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        int enabler_budget = std::max(1, 2 * seeded);
        int enabler_count = 0;
        for (const auto& [score, action] : enabler_candidates) {
            if (enabler_count >= enabler_budget) break;
            int t = target_timestep(action);
            if (activate_action_at(action, t)) {
                enabler_count++;
                if (config.is_verbose()) {
                    Logger::instance().info("  Seed enabler: " +
                        action->name() + " at t=" + std::to_string(t) +
                        " (ARPG layer=" +
                        std::to_string(achievers_->get_action_first_layer(action->id())) +
                        ", activity=" + std::to_string(score) + ")");
                }
            }
        }

        Logger::instance().info("Initial: " + std::to_string(seeded) +
            " goal achievers + " + std::to_string(enabler_count) +
            " cascade enablers across " + std::to_string(current_horizon_ + 1) +
            " timesteps (RPG lb: " + std::to_string(config.planner.start_timestep) +
            ", ARPG layers: " + std::to_string(arpg_layers) + "), " +
            std::to_string(problem_.actions().size()) + " actions" +
            ", h^ff RP: " + std::to_string(relaxed_plan_.size()) + " actions");
    }

    // 5. Check replenishment after seeding
    {
        bool need_ext = false;
        for (const Action& action : problem_.actions()) {
            if (blocked_count_[&action] <= 0) {
                need_ext = true;
                if (config.is_verbose()) {
                    Logger::instance().info("  Replenishment needed: " +
                        action.name() + " has no remaining blocked entries");
                }
            }
        }
        if (need_ext) {
            Logger::instance().info("  Extending horizon for replenishment (current_horizon=" +
                std::to_string(current_horizon_) + ")");
            extend_horizon();
        }
    }

    // 6. Main search loop: assumption-based solve → core-guided activation
    //    Each round: solve under assumptions, process UNSAT core (filter +
    //    activate + replenish), or extract plan on SAT. Activity scores
    //    decay by δ=0.85 after every round.
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
            stats.set("planner.total_activations", static_cast<double>(cumulative_core_activations_));

            // Count final active (unblocked) entries for activation tightness metric
            size_t final_active = 0;
            for (const auto& e : block_entries_) if (!e.active) final_active++;
            stats.set("planner.activated_entries", static_cast<double>(final_active));
            stats.set("planner.total_entries", static_cast<double>(block_entries_.size()));
            stats.set("planner.lazy_encoded_actions", static_cast<double>(action_encoded_.size()));
            size_t total_possible = problem_.actions().size() * static_cast<size_t>(current_horizon_ + 1);
            stats.set("planner.lazy_total_possible", static_cast<double>(total_possible));

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

            stats.add("planner.total_core_size", static_cast<double>(core.size()));
            stats.add("planner.core_count");

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
