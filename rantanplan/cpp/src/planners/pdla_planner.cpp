#include "pdla_planner.hpp"
#include "../encoders/grounded_encoder.hpp"
#include "../config/config.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include "../util/memory_tracker.hpp"
#include <algorithm>
#include <chrono>
#include <random>

namespace rantanplan {

// ===========================================================================
// PDLAPlanner — Property-Directed Lazy Activation
//
// Change 1: Float activation (per-action blocking).
// Change 2: Incremental activation (K per round, scored selection).
// Change 3: Obligation-driven search (backward chaining from goals).
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
// Achiever setup
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

    // Scoring normalization constants
    max_arpg_layer_ = std::max(1, achievers_->get_arpg_num_layers());
    max_effects_ = 1;
    for (const Action& a : problem_.actions()) {
        int ne = static_cast<int>(a.effects().size());
        if (ne > max_effects_) max_effects_ = ne;
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

    // Transitive achiever closure (diagnostics only)
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
}

// ---------------------------------------------------------------------------
// Float activation + lazy encoding (unchanged)
// ---------------------------------------------------------------------------

void PDLAPlanner::activate_action(const Action* action) {
    if (!blocked_.count(action)) return;
    blocked_.erase(action);
    activated_.insert(action);
    blk_id_to_action_.erase(block_lit_.at(action).id());
    for (int t = 0; t <= current_horizon_; t++) {
        encode_action_at(action, t);
    }
}

void PDLAPlanner::encode_action_at(const Action* action, int timestep) {
    auto& encoded = action_encoded_at_[action];
    if (encoded.count(timestep)) return;
    encoded.insert(timestep);

    auto effects = grounded_encoder().encode_single_action_effects_only(*action, timestep);
    if (effects) solver_.add(*effects);

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
    } else if (action->has_precondition()) {
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
    grounded_encoder().ensure_action_variables(t);
    solver_.add(*encoder_.encode_frames(t));
    solver_.add(*encoder_.encode_symmetries(t));
    propagator_strategy_->register_timestep_variables(t + 1);
    auto& vf = encoder_.get_variable_factory();
    for (const Action* a : blocked_) {
        const z3::expr& action_var = vf.get_action_variable(*a, t);
        solver_.add(z3::implies(block_lit_.at(a), !action_var));
    }
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
}

// ---------------------------------------------------------------------------
// Scoring function
// ---------------------------------------------------------------------------

double PDLAPlanner::score_action(const Action* action) const {
    double rp = relaxed_plan_set_.count(action) ? 1.0 : 0.0;

    int layer = achievers_->get_action_first_layer(action->id());
    double earliness = 1.0 - static_cast<double>(layer) / static_cast<double>(max_arpg_layer_);

    double readiness = 1.0;
    auto prec_it = action_precondition_ids_.find(action);
    if (prec_it != action_precondition_ids_.end() && !prec_it->second.empty()) {
        int total = static_cast<int>(prec_it->second.size());
        int satisfied = 0;
        for (ExprID p : prec_it->second) {
            if (init_satisfied_conditions_.count(p)) { satisfied++; continue; }
            auto ach_it = condition_achievers_.find(p);
            if (ach_it != condition_achievers_.end()) {
                for (const Action* enabler : ach_it->second) {
                    if (activated_.count(enabler)) { satisfied++; break; }
                }
            }
        }
        readiness = static_cast<double>(satisfied) / static_cast<double>(total);
    }

    double parsimony = 1.0 - std::min(1.0,
        static_cast<double>(action->effects().size()) / static_cast<double>(max_effects_));

    return 5.0 * rp + 2.0 * earliness + 3.0 * readiness + 1.0 * parsimony;
}

z3::expr_vector PDLAPlanner::build_assumptions() {
    // Collect blocking literals into a temporary vector and shuffle.
    // Z3's assumption ordering affects its internal search heuristics
    // (VSIDS initial activity, conflict clause ordering).  Shuffling
    // reduces deterministic bias and smooths out the variance from
    // Z3's sensitivity to assumption order — analogous to random
    // restarts in SAT solvers (Nadel 2010).
    std::vector<z3::expr> blk_lits;
    blk_lits.reserve(blocked_.size());
    for (const Action* a : blocked_) {
        blk_lits.push_back(block_lit_.at(a));
    }
    static std::mt19937 rng(42);  // fixed seed for reproducibility
    std::shuffle(blk_lits.begin(), blk_lits.end(), rng);

    z3::expr_vector assumptions(ctx_);
    for (auto& lit : blk_lits) {
        assumptions.push_back(lit);
    }
    assumptions.push_back(goal_assumption_);
    return assumptions;
}

bool PDLAPlanner::has_activated_achiever(ExprID condition) const {
    auto it = condition_achievers_.find(condition);
    if (it == condition_achievers_.end()) return false;
    for (const Action* a : it->second) {
        if (activated_.count(a)) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Phase A: obligation-driven activation
// ---------------------------------------------------------------------------

void PDLAPlanner::push_precondition_obligations(
        const Action* action, int deadline, int parent_depth) {
    auto prec_it = action_precondition_ids_.find(action);
    if (prec_it == action_precondition_ids_.end()) return;

    for (ExprID p : prec_it->second) {
        if (init_satisfied_conditions_.count(p)) continue;
        if (has_activated_achiever(p)) continue;
        obligation_queue_.push({p, std::max(0, deadline - 1), parent_depth + 1, action, false, 0.0});
    }
}

int PDLAPlanner::process_obligations() {
    // Split budget by obligation source — exploration vs. exploitation.
    //
    // Backward-chain obligations (from_core=false): NO budget.  These deepen
    // the causal chain for actions we've committed to.  "If we activated A,
    // we must satisfy A's preconditions."  Following the chain to completion
    // is exploitation — investing in a chosen path.
    //
    // Core-derived obligations (from_core=true): exactly 1 per round.  These
    // are alternatives — "the solver says the current achiever doesn't work,
    // try a different ground instance."  Processing one at a time maximizes
    // information gain: the solver's response to a single new activation
    // tells us precisely whether that was the right choice, or whether we
    // need a different alternative, or whether the real issue is horizon
    // length.  This follows IC3's discipline of one proof obligation per
    // solver call, and CEGAR's principle of one refinement per
    // counterexample.
    static constexpr int core_budget = 1;

    int activations = 0;
    int core_activations = 0;

    while (!obligation_queue_.empty()) {
        Obligation ob = obligation_queue_.top();

        if (ob.from_core && core_activations >= core_budget) break;

        obligation_queue_.pop();

        if (init_satisfied_conditions_.count(ob.condition)) continue;

        auto ach_it = condition_achievers_.find(ob.condition);
        if (ach_it == condition_achievers_.end()) {
            deferred_obligations_.push_back(ob);
            continue;
        }

        // Backward-chain obligations skip conditions already achievable.
        // Core-derived obligations don't: the solver proved it doesn't work.
        if (!ob.from_core && has_activated_achiever(ob.condition)) continue;

        const Action* best = nullptr;
        double best_score = -1e9;
        for (const Action* a : ach_it->second) {
            if (!blocked_.count(a)) continue;
            double s = score_action(a);
            if (s > best_score || (s == best_score && (!best || a->id() < best->id()))) {
                best_score = s;
                best = a;
            }
        }

        if (!best) {
            deferred_obligations_.push_back(ob);
            continue;
        }

        activate_action(best);
        action_activation_depth_[best] = ob.depth;
        activations++;
        if (ob.from_core) core_activations++;

        if (Config::instance().is_verbose()) {
            Logger::instance().info("  Obligation: c" + std::to_string(ob.condition.id) +
                " d=" + std::to_string(ob.depth) +
                (ob.from_core ? " [alt]" : "") +
                " → " + best->label() +
                " (score=" + std::to_string(best_score) + ")");
        }

        // Sub-obligations from this activation are backward-chain (not core)
        push_precondition_obligations(best, ob.deadline, ob.depth);
    }

    return activations;
}

// ---------------------------------------------------------------------------
// Phase B: core → obligations
// ---------------------------------------------------------------------------

PDLAPlanner::CoreResult PDLAPlanner::process_core_for_obligations(
        const z3::expr_vector& core) {
    CoreResult result{0, 0, 0};

    // ---- Source 1: tracked precondition failures ----
    //
    // Three-way classification:
    //   (a) No activated achiever → obligation (need a new action)
    //   (b) Activated achiever + blocked alternatives → from_core obligation
    //       (try a different ground instance)
    //   (c) Activated achiever + NO blocked alternatives → horizon signal
    //
    // For case (b), compute the timestep spread: the fraction of timesteps
    // at which the condition fails in this core.  High spread (fails at
    // many timesteps) means the solver exhaustively explored temporal
    // placements — a different achiever is unlikely to help.  The spread
    // is used as a priority signal: low-spread obligations are tried first.

    // Collect per-condition: representative TrackedPrecond + set of failing timesteps
    struct CondEntry {
        TrackedPrecond tp;          // representative (earliest timestep)
        std::unordered_set<int> failing_timesteps;
    };
    std::unordered_map<int, CondEntry> cond_entries;

    for (unsigned i = 0; i < core.size(); ++i) {
        unsigned eid = core[i].id();
        auto tp_it = tracked_precond_id_.find(eid);
        if (tp_it == tracked_precond_id_.end()) continue;
        auto& tp = tp_it->second;
        auto ce_it = cond_entries.find(tp.condition.id);
        if (ce_it == cond_entries.end()) {
            cond_entries[tp.condition.id] = {tp, {tp.timestep}};
        } else {
            ce_it->second.failing_timesteps.insert(tp.timestep);
            if (tp.timestep < ce_it->second.tp.timestep) {
                ce_it->second.tp = tp;
            }
        }
    }

    double horizon_denom = std::max(1.0, static_cast<double>(current_horizon_ + 1));

    for (auto& [cond_id, entry] : cond_entries) {
        auto& tp = entry.tp;
        if (init_satisfied_conditions_.count(tp.condition)) continue;

        int depth = 1;
        auto ad_it = action_activation_depth_.find(tp.action);
        if (ad_it != action_activation_depth_.end()) {
            depth = ad_it->second + 1;
        }

        bool has_activated = has_activated_achiever(tp.condition);
        bool has_blocked_alt = false;
        auto ach_it = condition_achievers_.find(tp.condition);
        if (ach_it != condition_achievers_.end()) {
            for (const Action* a : ach_it->second) {
                if (blocked_.count(a)) { has_blocked_alt = true; break; }
            }
        }

        double spread = static_cast<double>(entry.failing_timesteps.size()) / horizon_denom;

        if (!has_activated) {
            // (a) No achiever activated — genuinely need a new action
            obligation_queue_.push({tp.condition, tp.timestep, depth, tp.action, false, 0.0});
            result.new_obligations++;
            if (Config::instance().is_verbose()) {
                Logger::instance().info("  Core→need: " + tp.action->label() +
                    "@t" + std::to_string(tp.timestep) +
                    " c" + std::to_string(tp.condition.id) +
                    " (d=" + std::to_string(depth) + ")");
            }
        } else if (has_blocked_alt) {
            // (b) Achiever exists but solver can't use it — try alternative.
            // Spread used for priority: low spread = high priority.
            obligation_queue_.push({tp.condition, tp.timestep, depth, tp.action, true, spread});
            result.new_obligations++;
            if (Config::instance().is_verbose()) {
                Logger::instance().info("  Core→alt: " + tp.action->label() +
                    "@t" + std::to_string(tp.timestep) +
                    " c" + std::to_string(tp.condition.id) +
                    " (spread=" + std::to_string(spread) +
                    ", d=" + std::to_string(depth) + ")");
            }
        } else {
            // (c) All achievers already activated — resource/horizon exhaustion
            result.horizon_signals++;
            if (Config::instance().is_verbose()) {
                Logger::instance().info("  Core→horizon: " + tp.action->label() +
                    "@t" + std::to_string(tp.timestep) +
                    " c" + std::to_string(tp.condition.id));
            }
        }
    }

    // ---- Source 2: blocking literals ----

    std::vector<const Action*> core_blocked_actions;
    for (unsigned i = 0; i < core.size(); ++i) {
        unsigned eid = core[i].id();
        auto it = blk_id_to_action_.find(eid);
        if (it == blk_id_to_action_.end()) continue;
        core_blocked_actions.push_back(it->second);
    }

    // Try structured path: convert to condition obligations
    for (const Action* action : core_blocked_actions) {
        const auto& achieved = achievers_->get_achieved_conditions(*action);
        for (ExprID cond : achieved) {
            if (init_satisfied_conditions_.count(cond)) continue;
            if (has_activated_achiever(cond)) continue;
            obligation_queue_.push({cond, current_horizon_, 0, nullptr, true, 0.0});
            result.new_obligations++;
        }
    }

    // Blocking-literal activations: always activate, not gated on
    // whether structured processing produced obligations.
    //
    // Blocking literals and tracked preconditions are independent signals:
    //   - Blocking literals: "I want these actions available" (solver demand)
    //   - Tracked preconditions: "these conditions are failing" (diagnosis)
    //
    // The obligation queue handles tracked preconditions (structured,
    // selective, one alternative at a time).  Blocking literals are the
    // solver's direct demand for specific actions — suppressing them
    // because obligations were also produced loses information.
    //
    // This makes PDLA self-regulating:
    //   - Sparse domains: cores have few blocking literals → few direct
    //     activations.  The obligation queue handles most of the work.
    //   - Dense domains: cores have many blocking literals → bulk
    //     activation, converging to CE-like behavior.
    //   - The obligation queue's three-way classification still provides
    //     structured feedback (horizon signals, selective alternatives)
    //     on top of the direct activations.
    // Activate blocking literals: either always (solver demand is trusted)
    // or only as fallback (when structured processing produced nothing).
    bool do_direct = always_activate_core_ || (result.new_obligations == 0);
    if (do_direct) {
        for (const Action* a : core_blocked_actions) {
            if (!blocked_.count(a)) continue;
            activate_action(a);
            action_activation_depth_[a] = 0;
            result.direct_activations++;

            if (Config::instance().is_verbose()) {
                Logger::instance().info("  Core→direct: " + a->label());
            }
            push_precondition_obligations(a, current_horizon_, 0);
        }
    }

    return result;
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
        Logger::instance().info("Pre-extended horizon to " +
            std::to_string(current_horizon_) +
            " (RPG lower bound: " + std::to_string(start_ts) +
            ", relaxed plan: " + std::to_string(relaxed_plan_.size()) + " actions)");
    }

    // 4. Push goal conditions as initial obligations
    for (ExprID g : goal_condition_ids_) {
        if (!init_satisfied_conditions_.count(g)) {
            obligation_queue_.push({g, current_horizon_, 0, nullptr, false, 0.0});
        }
    }

    Logger::instance().info("Initial: " +
        std::to_string(obligation_queue_.size()) + " goal obligations, " +
        std::to_string(current_horizon_ + 1) + " timesteps, " +
        std::to_string(problem_.actions().size()) + " actions, " +
        "h^ff RP: " + std::to_string(relaxed_plan_.size()) + " actions");

    // 5. Main search loop: Phase A (obligations) → Phase B (solver)
    int round = 0;
    int extensions = 0;
    double total_time = 0.0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        if (!apply_solver_timeout(solver_)) break;

        // Phase A: process obligations
        int phase_a_activations = process_obligations();

        if (Config::instance().is_verbose() && phase_a_activations > 0) {
            Logger::instance().info("  Phase A: " + std::to_string(phase_a_activations) +
                " activations, queue=" + std::to_string(obligation_queue_.size()) +
                " deferred=" + std::to_string(deferred_obligations_.size()));
        }

        // Phase B: solver feasibility check
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
            {"queue", std::to_string(obligation_queue_.size())},
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
            stats.set("planner.activated_actions", static_cast<double>(activated_.size()));
            stats.set("planner.total_actions", static_cast<double>(problem_.actions().size()));

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
                    if (blk_id_to_action_.find(eid) != blk_id_to_action_.end())
                        core_blocking++;
                    else if (tracked_precond_id_.find(eid) != tracked_precond_id_.end())
                        core_tracked++;
                    else
                        core_other++;
                }
                Logger::instance().info("  Core: " + std::to_string(core.size()) +
                    " (blk=" + std::to_string(core_blocking) +
                    " trk=" + std::to_string(core_tracked) +
                    " oth=" + std::to_string(core_other) + ")");
            }

            stats.add("planner.total_core_size", static_cast<double>(core.size()));
            stats.add("planner.core_count");

            CoreResult cr = process_core_for_obligations(core);

            if (config.is_verbose()) {
                Logger::instance().info("  Core result: obs=" + std::to_string(cr.new_obligations) +
                    " horizon_signals=" + std::to_string(cr.horizon_signals) +
                    " direct=" + std::to_string(cr.direct_activations));
            }

            // Decide whether to extend the horizon.
            // Extend when: (1) no activation progress was made, AND either
            //   (a) core produced only horizon signals (resource exhaustion), or
            //   (b) queue is empty and nothing was produced at all.
            bool made_progress = (phase_a_activations > 0 ||
                                  cr.new_obligations > 0 ||
                                  cr.direct_activations > 0);

            bool horizon_exhausted = (!made_progress && cr.horizon_signals > 0);
            bool completely_stuck = (!made_progress && cr.horizon_signals == 0 &&
                                     obligation_queue_.empty());

            if (horizon_exhausted || completely_stuck) {
                if (current_horizon_ >= config.planner.max_steps) {
                    Logger::instance().info("Horizon limit reached (" +
                        std::to_string(current_horizon_) + ")");
                    break;
                }
                extensions++;
                Logger::instance().info("Extending horizon (" +
                    std::string(horizon_exhausted ? "resource exhaustion" : "stuck") +
                    ", #" + std::to_string(extensions) +
                    ", horizon=" + std::to_string(current_horizon_) + ")");
                extend_horizon();

                for (auto& ob : deferred_obligations_) {
                    ob.deadline = current_horizon_;
                    obligation_queue_.push(ob);
                }
                deferred_obligations_.clear();
            }

        } else {
            if (handle_unknown_result(solver_, "round " + std::to_string(round))) break;
        }

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
