#include "sequential.hpp"
#include "../config/config.hpp"
#include "../encoders/parallelism/graph.hpp"
#include "../encoders/parallelism/interference_analysis.hpp"
#include "propagators/null_propagator.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/stats.hpp"
#include "../util/logger.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <unordered_map>

namespace rantanplan {

    SequentialPlanner::SequentialPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
        : problem_(problem), encoder_(encoder), ctx_(ctx), solver_(ctx) {
        // Initialize planner with the given problem, encoder, and Z3 context
        // Propagator must be set via set_propagator_strategy() before search()
    }

    SequentialPlanner::SequentialPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx,
                                       std::unique_ptr<PropagatorStrategy> propagator)
        : problem_(problem), encoder_(encoder), ctx_(ctx), solver_(ctx),
          propagator_strategy_(std::move(propagator)) {
        // Initialize planner with the given problem, encoder, Z3 context, and propagator strategy
    }

void SequentialPlanner::debug_output_constraints() {
    // Output current constraints from the solver to SMT2 file
    std::ofstream smt2_file("output.smt2");
    if (smt2_file.is_open()) {
        smt2_file << ";; Current solver constraints from RantanPlan" << std::endl;
        smt2_file << solver_.to_smt2() << std::endl;
        smt2_file.close();
    } else {
        Logger::instance().error("Could not open output.smt2 for writing");
    }
}

void SequentialPlanner::add_timestep_constraints(int timestep) {
    BasePlanner::add_timestep_constraints(solver_, encoder_, *propagator_strategy_, timestep);
}

Plan SequentialPlanner::search() {
    auto& config = Config::instance();
    auto& stats = Stats::instance();

    int start_timestep = std::max(0, config.planner.start_timestep);

    // Initialise the horizon schedule from config.
    schedule_ = HorizonSchedule::from_string(config.planner.horizon_schedule);

    std::string search_msg = "Starting search with propagator: " + propagator_strategy_->get_name();
    if (start_timestep > 0) {
        search_msg += " from timestep " + std::to_string(start_timestep);
    }
    if (config.planner.horizon_schedule != "linear") {
        search_msg += ", schedule: " + config.planner.horizon_schedule;
    }
    Logger::instance().info(search_msg);

    solution_found_ = false;

    // Add initial state constraints (invariant across all horizons).
    solver_.add(*encoder_.encode_initial_state());
    propagator_strategy_->register_timestep_variables(0);

    // Pre-fill all transitions for [0, start_timestep - 1] so the first SAT call
    // begins from the RPG lower bound without redundant checks.
    for (int t = 0; t < start_timestep; ++t) {
        add_timestep_constraints(t);
    }

    // -------------------------------------------------------------------------
    // Schedule-driven search loop
    //
    // Key invariant: before each SAT call at horizon h, ALL transitions for
    // timesteps [0, h-1] are in the solver (encoding states 0..h).
    // The schedule determines *which* horizons receive a goal literal and a SAT
    // call; the inner loop fills every skipped transition unconditionally.
    //
    // Completeness (why one goal literal at h suffices for the whole batch):
    //   encode_prefix_monotone ensures actions are front-loaded in the model:
    //   if a plan of length k <= h exists, the solver represents it with actions
    //   at steps 0..k-1 and empty steps at k..h-1. Frame axioms propagate the
    //   goal state through the empty suffix so goal@h is satisfied. The single
    //   literal goal_at_h therefore witnesses any plan length in [prev_h+1, h].
    //
    // Plan length recovery after SAT:
    //   Scan forward from t=0 for the first step where all action variables are
    //   false. Because of front-loading, this is the exact plan boundary and
    //   requires no extra solver calls.
    // -------------------------------------------------------------------------

    double total_time = 0.0;
    auto start_time = std::chrono::high_resolution_clock::now();

    int filled_up_to = start_timestep;  // transitions [0, filled_up_to-1] are encoded
    int h             = start_timestep;  // first horizon to check

    while (h <= config.planner.max_steps) {
        // Check timeout
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration<double>(current_time - start_time).count();
        if (elapsed_seconds >= config.global.timeout) {
            Logger::instance().info("\n*** TIMEOUT reached after " +
                std::to_string(static_cast<int>(elapsed_seconds)) + "s ***");
            break;
        }

        auto step_start   = std::chrono::high_resolution_clock::now();
        auto formula_start = std::chrono::high_resolution_clock::now();

        // 1. Fill all skipped transitions [filled_up_to, h).
        //    When h == start_timestep (first iteration) or h == 0, the loop is
        //    empty — no transitions added, mirroring the original empty-plan check.
        for (int t = filled_up_to; t < h; ++t) {
            add_timestep_constraints(t);
        }
        filled_up_to = h;

        // 2. Place ONE goal literal at the scheduled horizon h.
        //    Intermediate horizons in the batch do NOT get goal literals; the
        //    prefix-monotone + frame-axiom argument above covers them.
        std::string goal_var_name = "goal_at_" + std::to_string(h);
        z3::expr goal_literal = ctx_.bool_const(goal_var_name.c_str());
        solver_.add(z3::implies(goal_literal, *encoder_.encode_goal(h)));

        z3::expr_vector assumptions(ctx_);
        assumptions.push_back(goal_literal);

        auto formula_end  = std::chrono::high_resolution_clock::now();
        auto formula_time = std::chrono::duration<double>(formula_end - formula_start).count();

        auto solve_start = std::chrono::high_resolution_clock::now();
        z3::check_result result = solver_.check(assumptions);
        auto solve_end  = std::chrono::high_resolution_clock::now();
        auto solve_time = std::chrono::duration<double>(solve_end - solve_start).count();

        auto step_end  = std::chrono::high_resolution_clock::now();
        auto step_time = std::chrono::duration<double>(step_end - step_start).count();

        total_time += step_time;

        stats.add("planner.timesteps_explored");
        stats.add("planner.formula_time", formula_time);
        stats.add("planner.solve_time", solve_time);
        stats.add("planner.total_time", step_time);

        double current_memory = MemoryTracker::instance().get_current_memory_mb();
        Logger::instance().timestep_solving(VerbosityLevel::INFO, h, {
            {"formula", std::to_string(formula_time) + "s"},
            {"solve",   std::to_string(solve_time) + "s"},
            {"step",    std::to_string(step_time) + "s"},
            {"mem",     std::to_string(static_cast<int>(current_memory)) + "MB"}
        });

        if (result == z3::sat) {
            z3::model model = solver_.get_model();
            solution_found_ = true;

            try {
                Plan plan = encoder_.extract_plan(model, h);

                Logger::instance().info("\n*** PLAN FOUND: horizon=" + std::to_string(h) +
                    ", actions=" + std::to_string(plan.length()) +
                    " (total time: " + std::to_string(total_time) + "s) ***");

                plan.write_ipc(config.planner.output_plan, 1,
                               -1.0, true,
                               config.planner.strategy, config.planner.mode, total_time);

                stats.set("planner.plan_length",      static_cast<double>(plan.length()));
                stats.set("planner.solution_horizon", static_cast<double>(h));
                collect_statistics();
                propagator_strategy_->cleanup();
                return plan;
            } catch (const std::exception& e) {
                Logger::instance().error("ERROR during plan extraction: " + std::string(e.what()));
                collect_statistics();
                propagator_strategy_->cleanup();
                return Plan();
            }

        } else if (result == z3::unsat) {
            // UNSAT at this horizon; advance to the next scheduled horizon.
        } else {
            Logger::instance().info("Solver returned unknown result at horizon " +
                std::to_string(h));
        }

        // Advance to next scheduled horizon.
        int next_h = schedule_.next(h, config.planner.max_steps);
        if (next_h <= h) break;  // capped at max_steps — no further progress possible
        h = next_h;
    }

    Logger::instance().info("\n*** NO PLAN FOUND within " +
        std::to_string(config.planner.max_steps) + " timesteps, aborting ***");
    Logger::instance().info("No plan found within " +
        std::to_string(config.planner.max_steps) + " timesteps.");

    collect_statistics();
    propagator_strategy_->cleanup();
    return Plan();
}


void SequentialPlanner::set_propagator_strategy(std::unique_ptr<PropagatorStrategy> propagator) {
    propagator_strategy_ = std::move(propagator);
}

std::string SequentialPlanner::get_propagator_strategy_name() const {
    return propagator_strategy_->get_name();
}

} // namespace rantanplan
