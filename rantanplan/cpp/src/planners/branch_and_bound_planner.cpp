#include "branch_and_bound_planner.hpp"
#include "../config/config.hpp"
#include "../encoders/base_encoder.hpp"
#include "propagators/null_propagator.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/stats.hpp"
#include "../util/logger.hpp"
#include <chrono>
#include <cmath>
#include <limits>

namespace rantanplan {

// ============================================================================
// Construction and utilities
// ============================================================================

BranchAndBoundPlanner::BranchAndBoundPlanner(
    const Problem& problem, BaseEncoder& encoder, z3::context& ctx,
    const InterferenceAnalysis* interference, SemanticsKind semantics)
    : BasePlanner(problem, encoder, ctx),
      abstract_(problem, ctx, encoder, interference, semantics) {
}

void BranchAndBoundPlanner::add_timestep_constraints(int timestep) {
    BasePlanner::add_timestep_constraints(solver_, encoder_, *propagator_strategy_, timestep);
}

/// Extract the numeric cost value from a Z3 model. Handles Z3's decimal string
/// representation, including the trailing '?' that Z3 appends for approximate values.
double BranchAndBoundPlanner::extract_cost_from_model(
    const z3::model& model, const z3::expr& cost_expr) {
    z3::expr val = model.eval(cost_expr, true);
    std::string decimal = val.get_decimal_string(10);

    // get_decimal_string may return "?" for non-numeric values
    if (decimal == "?" || decimal.empty()) return 0.0;

    // Remove trailing '?' that Z3 sometimes appends for approximate values
    if (!decimal.empty() && decimal.back() == '?') {
        decimal.pop_back();
    }

    try {
        return std::stod(decimal);
    } catch (...) {
        return 0.0;
    }
}

// ============================================================================
// Main search loop
//
// The B&B search interleaves two levels:
//   - Outer loop: iterates over horizons (h = number of concrete steps).
//     Concrete transition constraints are added permanently as h grows.
//   - Inner loop: at each horizon, repeatedly tightens the cost bound until
//     either a concrete solution is found (update upper bound, continue) or
//     no solution with cost < C* exists at this horizon (advance).
//
// The abstract suffix constraints are split into three categories following
// the same per-timestep pattern as the grounded encoder:
//
//   Permanent constraints (added once after initialize):
//     - mod_v equivalences (Eq. 4B) — horizon-independent
//     - Loop formulas (Eq. 5) — horizon-independent, cached
//
//   Horizon-scoped constraints (inside push/pop per horizon):
//     - Axiom 6 (Eq. 6) — no arbitrary deferral, per step
//     - Axiom 7 step (Eq. 7) — full concrete prefix, per step
//     - Relaxed preconditions (Eq. 4A) — evaluated at state h
//     - Abstract goal (Eq. 3) — evaluated at state h
//     - Cost bound and goal implications from the inner B&B loop
//
// Compared to the naive approach of push/pop around ALL suffix constraints,
// this avoids re-adding the permanent mod_v equivalences and loop formulas
// each horizon. The decomposed AbstractSuffixEncoder API also allows the
// planner to control exactly which constraints are permanent vs scoped.
// ============================================================================

Plan BranchAndBoundPlanner::search() {
    auto& config = Config::instance();
    auto& stats = Stats::instance();

    int start_timestep = std::max(0, config.planner.start_timestep);
    schedule_ = HorizonSchedule::from_string(config.planner.horizon_schedule);

    std::string search_msg = "B&B search with propagator: " + propagator_strategy_->get_name();
    if (start_timestep > 0) {
        search_msg += " from timestep " + std::to_string(start_timestep);
    }
    search_msg += ", timeout: " + format_timeout_string();
    Logger::instance().info(search_msg);

    solution_found_ = false;
    optimality_proven_ = false;
    timed_out_ = false;
    init_deadline();

    // Initialize
    solver_.add(*encoder_.encode_initial_state());
    propagator_strategy_->register_timestep_variables(0);
    abstract_.initialize();

    // Permanent abstract constraints (add once, like encode_actions/frames)
    solver_.add(abstract_.encode_mod_v_equivalences());
    solver_.add(abstract_.encode_loop_formulas());

    Plan best_plan;
    double upper_bound = std::numeric_limits<double>::infinity();
    int cost_bound_counter = 0;

    auto start_time = std::chrono::high_resolution_clock::now();
    double total_time = 0.0;

    // h = horizon = goal state index (same convention as SequentialPlanner).
    // At horizon h we encode transitions [0, h-1], producing states 0..h,
    // and check the goal at state h with the abstract suffix starting from h.
    int filled_up_to = 0;
    int h = start_timestep;

    // Pre-fill transitions up to start_timestep
    for (int t = 0; t < h; ++t) {
        add_timestep_constraints(t);
        filled_up_to = t + 1;
    }

    while (h <= config.planner.max_steps) {
        // Check timeout before building formula
        if (!apply_solver_timeout(solver_)) break;

        auto step_start = std::chrono::high_resolution_clock::now();

        // 1. Fill concrete transitions up to h-1 (producing states 0..h)
        for (int t = filled_up_to; t < h; ++t) {
            add_timestep_constraints(t);
        }
        filled_up_to = h;

        // 2. Push scope for horizon-dependent constraints. Permanent constraints
        //    (mod_v equivalences, loop formulas) remain outside and are not re-added.
        solver_.push();

        // Add axioms for all steps up to h (inside scope — they get popped
        // and re-added each horizon, same as original, but now using the
        // decomposed per-step methods)
        for (int i = 0; i < h; ++i) {
            if (i > 0) {
                solver_.add(abstract_.encode_axiom_6(i));
            }
            solver_.add(abstract_.encode_axiom_7_step(i));
        }

        solver_.add(abstract_.encode_relaxed_preconditions(h));
        solver_.add(abstract_.encode_abstract_goal(h));

        // 4. Build total cost expression and goal for this horizon
        z3::expr total_cost = abstract_.build_total_cost(h);

        std::string goal_name = "goal_at_" + std::to_string(h);
        z3::expr goal_lit = ctx_.bool_const(goal_name.c_str());
        solver_.add(z3::implies(goal_lit, *encoder_.encode_goal(h)));

        // 5. Inner B&B loop at this horizon
        bool advance_horizon = false;
        while (!advance_horizon) {
            // Apply Z3 timeout for the inner loop check too
            if (!apply_solver_timeout(solver_)) {
                advance_horizon = true;
                break;
            }

            // Create cost bound assumption
            std::string cost_name = "cost_lt_" + std::to_string(cost_bound_counter++);
            z3::expr cost_bound_lit = ctx_.bool_const(cost_name.c_str());
            bool have_bound = !std::isinf(upper_bound);
            if (have_bound) {
                solver_.add(z3::implies(cost_bound_lit,
                    total_cost < ctx_.real_val(std::to_string(upper_bound).c_str())));
            }

            z3::expr_vector assumptions(ctx_);
            assumptions.push_back(goal_lit);
            if (have_bound) {
                assumptions.push_back(cost_bound_lit);
            }

            auto solve_start = std::chrono::high_resolution_clock::now();
            z3::check_result result = solver_.check(assumptions);
            auto solve_end = std::chrono::high_resolution_clock::now();
            auto solve_time = std::chrono::duration<double>(solve_end - solve_start).count();

            stats.add("planner.timesteps_explored");
            stats.add("planner.solve_time", solve_time);

            double current_memory = MemoryTracker::instance().get_current_memory_mb();
            Logger::instance().timestep_solving(VerbosityLevel::INFO, h, {
                {"solve", std::to_string(solve_time) + "s"},
                {"bound", have_bound ? std::to_string(upper_bound) : "inf"},
                {"mem",   std::to_string(static_cast<int>(current_memory)) + "MB"}
            });

            if (result == z3::unsat) {
                if (std::isinf(upper_bound)) {
                    if (h == 0) {
                        Logger::instance().info(
                            "\n*** UNSOLVABLE_PROVEN: no plan exists ***");
                        collect_statistics();
                        propagator_strategy_->cleanup();
                        return Plan();
                    }
                    advance_horizon = true;
                } else {
                    // GLOBALLY OPTIMAL
                    optimality_proven_ = true;
                    best_cost_ = upper_bound;
                    auto end_time = std::chrono::high_resolution_clock::now();
                    total_time = std::chrono::duration<double>(end_time - start_time).count();
                    Logger::instance().info(
                        "\n*** OPTIMAL PLAN FOUND: horizon=" + std::to_string(h) +
                        ", actions=" + std::to_string(best_plan.length()) +
                        ", cost=" + std::to_string(upper_bound) +
                        " (total time: " + std::to_string(total_time) + "s) ***");
                    collect_statistics();
                    propagator_strategy_->cleanup();
                    return best_plan;
                }
            } else if (result == z3::sat) {
                z3::model model = solver_.get_model();

                if (abstract_.is_concrete_solution(model)) {
                    double plan_cost = extract_cost_from_model(model, total_cost);
                    upper_bound = plan_cost;
                    best_plan = encoder_.extract_plan(model, h);
                    solution_found_ = true;
                    solution_count_++;

                    auto now = std::chrono::high_resolution_clock::now();
                    double elapsed = std::chrono::duration<double>(now - start_time).count();
                    bool unit = (problem_.metric_kind() != MetricKind::MINIMIZE_ACTION_COSTS);
                    best_plan.write_ipc(config.planner.output_plan, solution_count_,
                                        upper_bound, unit,
                                        config.planner.strategy, config.planner.mode, elapsed);

                    Logger::instance().info(
                        "  Improved plan: cost=" + std::to_string(upper_bound) +
                        ", horizon=" + std::to_string(h) +
                        ", actions=" + std::to_string(best_plan.length()));
                    // Continue inner loop to find cheaper plan at same horizon
                } else {
                    // Abstract solution — this horizon exhausted
                    advance_horizon = true;
                }
            } else {
                handle_unknown_result(solver_, "B&B horizon " + std::to_string(h));
                advance_horizon = true;
            }
        }

        // 6. Pop horizon-dependent scope, advance horizon
        solver_.pop();
        auto step_end = std::chrono::high_resolution_clock::now();
        auto step_time = std::chrono::duration<double>(step_end - step_start).count();
        total_time += step_time;
        stats.add("planner.total_time", step_time);

        if (timed_out_) break;

        int next_h = schedule_.next(h, config.planner.max_steps);
        if (next_h <= h) break;
        h = next_h;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    total_time = std::chrono::duration<double>(end_time - start_time).count();

    if (solution_found_) {
        best_cost_ = upper_bound;
        std::string reason = timed_out_ ? "timeout" : "search limit";
        Logger::instance().info(
            "\n*** BEST PLAN FOUND: horizon=" + std::to_string(h) +
            ", actions=" + std::to_string(best_plan.length()) +
            ", cost=" + std::to_string(upper_bound) +
            " (optimality not proven — " + reason +
            ", total time: " + std::to_string(total_time) + "s) ***");
    } else if (timed_out_) {
        Logger::instance().info(
            "\n*** NO PLAN FOUND (timeout after " +
            std::to_string(total_time) + "s) ***");
    } else {
        Logger::instance().info(
            "\n*** NO PLAN FOUND within " +
            std::to_string(config.planner.max_steps) +
            " timesteps (total time: " + std::to_string(total_time) + "s) ***");
    }

    collect_statistics();
    propagator_strategy_->cleanup();
    return best_plan;
}

} // namespace rantanplan
