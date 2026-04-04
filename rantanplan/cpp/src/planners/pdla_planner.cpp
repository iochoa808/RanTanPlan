#include "pdla_planner.hpp"
#include "../encoders/grounded_encoder.hpp"
#include "../config/config.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include "../util/memory_tracker.hpp"
#include <algorithm>
#include <chrono>
#include <limits>

namespace rantanplan {

// ===========================================================================
// PDLAPlanner — Property-Directed Lazy Activation
//
// Float activation: one blocking literal per action (not per timestep).
//   blk_a → ¬act_a_t0 ∧ ¬act_a_t1 ∧ ...
// Activating an action removes its blk_a from assumptions, making it
// available at ALL timesteps.  Constraints are lazily encoded at each
// timestep on activation.
// ===========================================================================

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PDLAPlanner::PDLAPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
    : BasePlanner(problem, encoder, ctx),
      goal_assumption_(ctx.bool_val(true)) {
}

GroundedEncoder& PDLAPlanner::grounded_encoder() {
    if (!grounded_encoder_) {
        grounded_encoder_ = &dynamic_cast<GroundedEncoder&>(encoder_);
    }
    return *grounded_encoder_;
}

void PDLAPlanner::set_achievers(std::unique_ptr<AchieversAnalysis> achievers) {
    achievers_ = std::move(achievers);
}

// ---------------------------------------------------------------------------
// Achiever setup (unchanged from CausalExistsPlanner)
// ---------------------------------------------------------------------------

void PDLAPlanner::build_action_id_map() {
    for (const Action& a : problem_.actions()) {
        action_id_to_ptr_[a.id()] = &a;
    }
}

void PDLAPlanner::build_condition_achiever_cache() {
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

void PDLAPlanner::compute_init_satisfied_conditions() {
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

void PDLAPlanner::initialize_achievers() {
    auto init_start = std::chrono::high_resolution_clock::now();

    build_action_id_map();

    if (!achievers_) {
        achievers_ = std::make_unique<AchieversAnalysis>(problem_);
    } else {
        Logger::instance().info("PDLA: reusing pre-built AchieversAnalysis from pipeline");
    }

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

    Logger::instance().info("PDLA achiever analysis: " +
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

    extract_relaxed_plan();
}

void PDLAPlanner::extract_relaxed_plan() {
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
                relaxed_plan_set_.insert(best);
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
                " (RPG layer=" + std::to_string(achievers_->get_action_first_layer(a->id())) + ")");
        }
    }
}

// ---------------------------------------------------------------------------
// Float activation: per-action blocking + lazy encoding
// ---------------------------------------------------------------------------

void PDLAPlanner::activate_action(const Action* action) {
    if (!blocked_.count(action)) return;  // already activated

    blocked_.erase(action);
    activated_.insert(action);
    blk_id_to_action_.erase(block_lit_.at(action).id());

    // Encode constraints at all existing timesteps
    for (int t = 0; t <= current_horizon_; t++) {
        encode_action_at(action, t);
    }
}

void PDLAPlanner::encode_action_at(const Action* action, int timestep) {
    auto& encoded = action_encoded_at_[action];
    if (encoded.count(timestep)) return;
    encoded.insert(timestep);

    // Effects (normal assertion)
    auto effects = grounded_encoder().encode_single_action_effects_only(*action, timestep);
    if (effects) {
        solver_.add(*effects);
    }

    // Preconditions: tracked per-condition
    auto prec_it = action_precondition_ids_.find(action);
    if (prec_it != action_precondition_ids_.end()) {
        auto& vf = encoder_.get_variable_factory();
        const z3::expr& action_var = vf.get_action_variable(*action, timestep);

        for (ExprID cond : prec_it->second) {
            std::string track_name = "pre_a" + std::to_string(action->id()) +
                                     "_t" + std::to_string(timestep) +
                                     "_c" + std::to_string(cond.id);
            z3::expr track_lit = ctx_.bool_const(track_name.c_str());
            z3::expr cond_z3 = encoder_.convert_expr_id_to_z3(cond, timestep);
            solver_.add(z3::implies(action_var, cond_z3), track_lit);
            tracked_precond_id_[track_lit.id()] = {action, timestep, cond};
        }
    }
    // Safety net: actions not in the achiever cache get precondition-only encoding
    else if (action->has_precondition()) {
        auto& vf = encoder_.get_variable_factory();
        const z3::expr& action_var = vf.get_action_variable(*action, timestep);
        z3::expr prec_z3 = encoder_.convert_expr_id_to_z3(action->precondition_id(), timestep);
        solver_.add(z3::implies(action_var, prec_z3));
    }
}

// ---------------------------------------------------------------------------
// Timestep management
// ---------------------------------------------------------------------------

void PDLAPlanner::add_timestep(int t) {
    // Eager skeleton: action variables, frames, symmetries
    grounded_encoder().ensure_action_variables(t);
    solver_.add(*encoder_.encode_frames(t));
    solver_.add(*encoder_.encode_symmetries(t));
    propagator_strategy_->register_timestep_variables(t + 1);

    auto& vf = encoder_.get_variable_factory();

    // Link blocking literals to new timestep for still-blocked actions
    for (const Action* a : blocked_) {
        const z3::expr& action_var = vf.get_action_variable(*a, t);
        solver_.add(z3::implies(block_lit_.at(a), !action_var));
    }

    // Encode constraints at new timestep for already-activated actions
    for (const Action* a : activated_) {
        encode_action_at(a, t);
    }
}

void PDLAPlanner::refresh_goal(int t) {
    goal_timestep_ = t;
    int version = next_goal_version_++;
    std::string lit_name = "goal_v" + std::to_string(version);
    goal_assumption_ = ctx_.bool_const(lit_name.c_str());
    solver_.add(z3::implies(goal_assumption_, *encoder_.encode_goal(t)));
}

void PDLAPlanner::extend_horizon() {
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

z3::expr_vector PDLAPlanner::build_assumptions() {
    z3::expr_vector assumptions(ctx_);

    // One blocking literal per still-blocked action
    for (const Action* a : blocked_) {
        assumptions.push_back(block_lit_.at(a));
    }

    assumptions.push_back(goal_assumption_);
    return assumptions;
}

const Action* PDLAPlanner::select_best_achiever_for(
        const Action* core_action) const {
    if (!achievers_) return nullptr;

    const auto& achieved = achievers_->get_achieved_conditions(*core_action);

    std::unordered_set<const Action*> candidates;
    for (ExprID cond : achieved) {
        if (init_satisfied_conditions_.count(cond)) continue;

        auto cach_it = condition_achievers_.find(cond);
        if (cach_it == condition_achievers_.end()) continue;

        for (const Action* alt : cach_it->second) {
            candidates.insert(alt);
        }
    }

    if (candidates.empty()) return nullptr;

    const Action* best = nullptr;
    double best_score = -1e9;

    for (const Action* action : candidates) {
        if (!goal_relevant_actions_.count(action)) continue;
        if (!blocked_.count(action)) continue;  // must still be blocked

        double score = 0;

        if (relaxed_plan_set_.count(action)) score += 20;

        auto act_it = activity_.find(action);
        if (act_it != activity_.end()) score += act_it->second * 5;

        auto prec_it = action_precondition_ids_.find(action);
        if (prec_it != action_precondition_ids_.end()) {
            for (ExprID p : prec_it->second) {
                if (init_satisfied_conditions_.count(p)) continue;

                bool has_enabler = false;
                auto ach_it = condition_achievers_.find(p);
                if (ach_it != condition_achievers_.end()) {
                    for (const Action* enabler : ach_it->second) {
                        if (activated_.count(enabler)) {
                            has_enabler = true;
                            break;
                        }
                    }
                }

                score -= has_enabler ? 5 : 30;
            }
        }

        score -= static_cast<double>(action->effects().size()) * 2;

        if (score > best_score) {
            best_score = score;
            best = action;
        }
    }

    return best;
}

int PDLAPlanner::process_core(const z3::expr_vector& core) {
    int activated = 0;
    int filtered = 0;
    std::vector<const Action*> bumped_actions;

    for (unsigned i = 0; i < core.size(); ++i) {
        unsigned eid = core[i].id();
        auto it = blk_id_to_action_.find(eid);
        if (it == blk_id_to_action_.end()) continue;

        const Action* action = it->second;

        if (!goal_relevant_actions_.count(action)) {
            filtered++;
            continue;
        }

        const Action* to_activate = action;

        // Guided activation: substitute with a better achiever if available
        {
            const Action* best = select_best_achiever_for(action);
            if (best && best != action && blocked_.count(best)) {
                activate_action(best);
                activity_[best] += 1.0;
                bumped_actions.push_back(best);
                activated++;
                guided_substitutions_++;
                to_activate = nullptr;
            }
        }

        if (to_activate && blocked_.count(to_activate)) {
            activate_action(to_activate);
            activity_[action] += 1.0;
            bumped_actions.push_back(action);
            activated++;
            guided_fallbacks_++;
        }
    }

    if (Config::instance().is_verbose() && filtered > 0) {
        Logger::instance().info("  Filtered: " + std::to_string(filtered) +
            " non-achiever blocking lits from core");
    }

    // Process tracked precondition failures
    std::unordered_map<int, TrackedPrecond> precond_earliest;
    for (unsigned i = 0; i < core.size(); ++i) {
        unsigned eid = core[i].id();
        auto tp_it = tracked_precond_id_.find(eid);
        if (tp_it == tracked_precond_id_.end()) continue;
        auto& tp = tp_it->second;
        auto pe_it = precond_earliest.find(tp.condition.id);
        if (pe_it == precond_earliest.end() || tp.timestep < pe_it->second.timestep) {
            precond_earliest[tp.condition.id] = tp;
        }
    }

    for (auto& [cond_id, tp] : precond_earliest) {
        auto ach_it = condition_achievers_.find(tp.condition);
        if (ach_it == condition_achievers_.end()) continue;

        const Action* best_enabler = nullptr;
        double best_score = -1e9;

        for (const Action* enabler : ach_it->second) {
            if (!goal_relevant_actions_.count(enabler)) continue;
            if (!blocked_.count(enabler)) continue;

            double score = 0;
            if (relaxed_plan_set_.count(enabler)) score += 20;
            auto ai = activity_.find(enabler);
            if (ai != activity_.end()) score += ai->second * 5;
            score -= static_cast<double>(enabler->effects().size()) * 2;

            if (score > best_score) {
                best_score = score;
                best_enabler = enabler;
            }
        }

        if (best_enabler) {
            activate_action(best_enabler);
            activity_[best_enabler] += 1.0;
            bumped_actions.push_back(best_enabler);
            activated++;

            if (Config::instance().is_verbose()) {
                Logger::instance().info("  Precond: " +
                    tp.action->label() + "@t" + std::to_string(tp.timestep) +
                    " needs c" + std::to_string(tp.condition.id) +
                    " → " + best_enabler->label());
            }
        }
    }

    cumulative_core_activations_ += activated;
    return activated;
}

void PDLAPlanner::cascade_bump(
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
        }
    }
}

int PDLAPlanner::predictive_activate(int /*timestep*/) {
    // With float activation, predictive activation operates on blocked
    // actions globally (not per-timestep). Budget and threshold unchanged.
    int budget = std::max(1, cumulative_core_activations_);

    std::vector<std::pair<double, const Action*>> candidates;
    for (const Action* a : blocked_) {
        double score = activity_[a];
        if (score <= activation_threshold_) continue;
        candidates.push_back({score, a});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    int act_count = 0;
    for (const auto& [score, action] : candidates) {
        if (act_count >= budget) break;

        activate_action(action);
        act_count++;

        if (Config::instance().is_verbose()) {
            Logger::instance().info("    Predictive activate: " +
                action->name() +
                " (activity=" + std::to_string(score) +
                ", budget=" + std::to_string(budget) + ")");
        }
    }

    return act_count;
}

void PDLAPlanner::decay_activity() {
    for (auto& [action, score] : activity_) {
        score *= activity_decay_;
    }
}

Plan PDLAPlanner::extract_plan(const z3::model& model) {
    return encoder_.extract_plan(model, current_horizon_ + 1);
}

// ---------------------------------------------------------------------------
// Main search
// ---------------------------------------------------------------------------

Plan PDLAPlanner::search() {
    auto& config = Config::instance();
    auto& stats = Stats::instance();

    solution_found_ = false;
    timed_out_ = false;
    init_deadline();

    Logger::instance().info("Starting PDLA search, timeout: " + format_timeout_string());

    // 0. Create per-action blocking literals
    for (const Action& a : problem_.actions()) {
        z3::expr blk = ctx_.bool_const(("blk_a" + std::to_string(a.id())).c_str());
        const Action* ptr = &a;
        block_lit_.insert({ptr, blk});
        blocked_.insert(ptr);
        blk_id_to_action_[blk.id()] = ptr;
    }

    // 1. Encode initial state
    solver_.add(*encoder_.encode_initial_state());
    propagator_strategy_->register_timestep_variables(0);

    // 2. Initialize achiever analysis
    initialize_achievers();

    // 3. Pre-extend horizon
    {
        int start_ts = config.planner.start_timestep;
        int num_timesteps = std::max(1, start_ts);

        for (int t = 0; t < num_timesteps; t++) {
            current_horizon_++;
            add_timestep(current_horizon_);
        }
        refresh_goal(current_horizon_ + 1);

        stats.set("planner.rpg_lower_bound", static_cast<double>(start_ts));
        stats.set("planner.initial_horizon", static_cast<double>(current_horizon_));

        Logger::instance().info("Pre-extended horizon to " +
            std::to_string(current_horizon_) +
            " (RPG lower bound: " + std::to_string(start_ts) +
            ", relaxed plan: " + std::to_string(relaxed_plan_.size()) + " actions)");
    }

    // 4. RPG-guided seeding
    {
        int max_goal_layer = 0;
        for (const Action* action : goal_achiever_actions_) {
            max_goal_layer = std::max(max_goal_layer,
                achievers_->get_action_first_layer(action->id()));
        }

        // Seed goal achievers (float activation — available at all timesteps)
        int seeded = 0;
        for (const Action* action : goal_achiever_actions_) {
            if (blocked_.count(action)) {
                activate_action(action);
                activity_[action] = 1.0;
                seeded++;
                if (config.is_verbose()) {
                    Logger::instance().info("  Seed goal achiever: " +
                        action->name() +
                        " (ARPG layer=" +
                        std::to_string(achievers_->get_action_first_layer(action->id())) + ")");
                }
            }
        }

        // Cascade bump from goal achievers
        std::vector<const Action*> seeds(goal_achiever_actions_.begin(),
                                         goal_achiever_actions_.end());
        cascade_bump(seeds, 1.0);

        // Activate cascade-bumped enablers
        std::vector<std::pair<double, const Action*>> enabler_candidates;
        for (auto& [action, score] : activity_) {
            if (score <= activation_threshold_) continue;
            if (goal_achiever_actions_.count(action)) continue;
            if (!blocked_.count(action)) continue;
            enabler_candidates.push_back({score, action});
        }
        std::sort(enabler_candidates.begin(), enabler_candidates.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        int enabler_budget = std::max(1, 2 * seeded);
        int enabler_count = 0;
        for (const auto& [score, action] : enabler_candidates) {
            if (enabler_count >= enabler_budget) break;
            activate_action(action);
            enabler_count++;
            if (config.is_verbose()) {
                Logger::instance().info("  Seed enabler: " +
                    action->name() +
                    " (RPG layer=" +
                    std::to_string(achievers_->get_action_first_layer(action->id())) +
                    ", activity=" + std::to_string(score) + ")");
            }
        }

        Logger::instance().info("Initial: " + std::to_string(seeded) +
            " goal achievers + " + std::to_string(enabler_count) +
            " cascade enablers, " +
            std::to_string(current_horizon_ + 1) + " timesteps" +
            " (RPG lb: " + std::to_string(config.planner.start_timestep) +
            ", max goal layer: " + std::to_string(max_goal_layer) + "), " +
            std::to_string(problem_.actions().size()) + " actions" +
            ", h^ff RP: " + std::to_string(relaxed_plan_.size()) + " actions" +
            ", blocked: " + std::to_string(blocked_.size()) +
            ", activated: " + std::to_string(activated_.size()));
    }

    // 5. Main search loop
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

        double mem = MemoryTracker::instance().get_current_memory_mb();

        Logger::instance().timestep_solving(VerbosityLevel::INFO, round, {
            {"solve", std::to_string(solve_time) + "s"},
            {"round", std::to_string(round_time) + "s"},
            {"horizon", std::to_string(current_horizon_)},
            {"activated", std::to_string(activated_.size()) + "/" + std::to_string(problem_.actions().size())},
            {"assumptions", std::to_string(assumptions.size())},
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
                "horizon=" + std::to_string(current_horizon_) +
                ", activated=" + std::to_string(activated_.size()) +
                "/" + std::to_string(problem_.actions().size()) +
                " (total time: " + std::to_string(total_time) + "s) ***");

            stats.set("planner.plan_length", static_cast<double>(plan.length()));
            stats.set("planner.rounds", static_cast<double>(round));
            stats.set("planner.solution_horizon", static_cast<double>(current_horizon_));
            stats.set("planner.total_activations", static_cast<double>(cumulative_core_activations_));
            stats.set("guided.substitutions", static_cast<double>(guided_substitutions_));
            stats.set("guided.fallbacks", static_cast<double>(guided_fallbacks_));
            stats.set("planner.activated_actions", static_cast<double>(activated_.size()));
            stats.set("planner.total_actions", static_cast<double>(problem_.actions().size()));

            // Count encoded (action, timestep) pairs
            size_t total_encoded = 0;
            for (const auto& [a, ts_set] : action_encoded_at_) {
                total_encoded += ts_set.size();
            }
            stats.set("planner.lazy_encoded_pairs", static_cast<double>(total_encoded));
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
                int core_blocking = 0, core_tracked = 0, core_other = 0;
                for (unsigned ci = 0; ci < core.size(); ++ci) {
                    unsigned eid = core[ci].id();
                    if (blk_id_to_action_.find(eid) != blk_id_to_action_.end()) {
                        core_blocking++;
                    } else if (tracked_precond_id_.find(eid) != tracked_precond_id_.end()) {
                        core_tracked++;
                    } else {
                        core_other++;
                    }
                }
                Logger::instance().info("  Core size: " + std::to_string(core.size()) +
                    " (blocking=" + std::to_string(core_blocking) +
                    ", tracked=" + std::to_string(core_tracked) +
                    ", other=" + std::to_string(core_other) + ")");
            }

            stats.add("planner.total_core_size", static_cast<double>(core.size()));
            stats.add("planner.core_count");

            int act = process_core(core);

            if (config.is_verbose()) {
                Logger::instance().info("  Activated: " + std::to_string(act));
            }

            if (act == 0) {
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
