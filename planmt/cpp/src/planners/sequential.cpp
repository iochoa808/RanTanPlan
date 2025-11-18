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

namespace planmt {

    SequentialPlanner::SequentialPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
        : problem_(problem), encoder_(encoder), ctx_(ctx), solver_(ctx),
          propagator_strategy_(std::make_unique<NullPropagator>()) {
        // Initialize planner with the given problem, encoder, and Z3 context
        // Uses null propagator by default (no propagation)
    }

    SequentialPlanner::SequentialPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx,
                                       std::unique_ptr<PropagatorStrategy> propagator)
        : problem_(problem), encoder_(encoder), ctx_(ctx), solver_(ctx),
          propagator_strategy_(propagator ? std::move(propagator) : std::make_unique<NullPropagator>()) {
        // Initialize planner with the given problem, encoder, Z3 context, and propagator strategy
    }

void SequentialPlanner::debug_output_constraints() {
    // Output current constraints from the solver to SMT2 file
    std::ofstream smt2_file("output.smt2");
    if (smt2_file.is_open()) {
        smt2_file << ";; Current solver constraints from planMT" << std::endl;
        smt2_file << solver_.to_smt2() << std::endl;
        smt2_file.close();
    } else {
        Logger::instance().error("Could not open output.smt2 for writing");
    }
}

void SequentialPlanner::collect_statistics() {
    auto& stats = Stats::instance();

    // Collect key Z3 solver statistics (just the important numbers)
    z3::stats z3_stats = solver_.statistics();
    for (unsigned i = 0; i < z3_stats.size(); ++i) {
        std::string key = "z3." + z3_stats.key(i);

        if (z3_stats.is_uint(i)) {
            stats.set(key, static_cast<double>(z3_stats.uint_value(i)));
        } else if (z3_stats.is_double(i)) {
            stats.set(key, z3_stats.double_value(i));
        }
    }

    // Add memory info
    stats.set("memory.current_mb", MemoryTracker::instance().get_current_memory_mb());
}

void SequentialPlanner::add_timestep_constraints(int timestep) {
    auto& config = Config::instance();

    solver_.add(*encoder_.encode_actions(timestep));
    solver_.add(*encoder_.encode_frames(timestep));

    // Add symmetry breaking constraints if enabled
    if (config.symmetry.detect_symmetries) {
        solver_.add(*encoder_.encode_symmetries(timestep));
    }

    // Only add parallelism constraints if propagator doesn't manage them
    if (!propagator_strategy_->manages_parallelism_constraints()) {
        solver_.add(*encoder_.encode_parallelism(timestep));
    }

    // Register variables for next timestep
    propagator_strategy_->register_timestep_variables(timestep + 1);
}

Plan SequentialPlanner::search() {
    auto& config = Config::instance();
    auto& stats = Stats::instance();

    int start_timestep = std::max(0, config.planner.start_timestep);

    std::string search_msg = "Starting search with propagator: " + propagator_strategy_->get_name();
    if (start_timestep > 0) {
        search_msg += " from timestep " + std::to_string(start_timestep);
    }
    Logger::instance().info(search_msg);
    
    // Reset solution found flag and statistics
    solution_found_ = false;
    //stats.clear();

    // Add initial state constraints (these are invariant)
    solver_.add(*encoder_.encode_initial_state());

    // Register variables for timestep 0 (initial state)
    propagator_strategy_->register_timestep_variables(0);

    // Pre-add all constraints for timesteps 1 through start_timestep-1
    // This ensures all necessary constraints are in place before starting the main search
    for (int pre_timestep = 1; pre_timestep < start_timestep; ++pre_timestep) {
        add_timestep_constraints(pre_timestep - 1);
    }

    // Now that we are all set up, iterate from start_timestep to max_steps timesteps and properly search
    double total_time = 0.0; // Track total time used
    auto start_time = std::chrono::high_resolution_clock::now(); // Track total search time for timeout
    for (int timestep = start_timestep; timestep <= config.planner.max_steps; ++timestep) {
        // Check timeout
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration<double>(current_time - start_time).count();
        if (elapsed_seconds >= config.global.timeout) {
            Logger::instance().info("\n*** TIMEOUT reached after " + std::to_string(static_cast<int>(elapsed_seconds)) + "s ***");
            break;
        }
        auto step_start = std::chrono::high_resolution_clock::now();
        
        // Time formula creation
        auto formula_start = std::chrono::high_resolution_clock::now();
        
        // This check is needed in case start_timestep_ is 0 as the first check will be for timestep 0
        // and we want to check if the goal is satisfiable in the initial state
        if (timestep > 0) { 
            // Add constraints for this timestep (incremental)
            add_timestep_constraints(timestep - 1);
        }
        
        // Make the goal literal imply the goal at this timestep
        std::string goal_var_name = "goal_at_" + std::to_string(timestep);
        z3::expr goal_literal = ctx_.bool_const(goal_var_name.c_str());
        // goal_at_3 -> at_package1_location_3 /\ at_package2_location_3
        solver_.add(z3::implies(goal_literal, *encoder_.encode_goal(timestep)));
        
        // Check satisfiability by assuming a reified goal literal
        z3::expr_vector assumptions(ctx_);
        assumptions.push_back(goal_literal);
        
        auto formula_end = std::chrono::high_resolution_clock::now();
        auto formula_time = std::chrono::duration<double>(formula_end - formula_start).count();

        //debug_output_constraints(); // Output initial constraints

        // Time solving
        auto solve_start = std::chrono::high_resolution_clock::now();
        z3::check_result result = solver_.check(assumptions);
        auto solve_end = std::chrono::high_resolution_clock::now();
        auto solve_time = std::chrono::duration<double>(solve_end - solve_start).count();

        auto step_end = std::chrono::high_resolution_clock::now();
        auto step_time = std::chrono::duration<double>(step_end - step_start).count();

        total_time += step_time; // Accumulate total time

        // Collect basic statistics
        stats.add("planner.timesteps_explored");
        stats.add("planner.formula_time", formula_time);
        stats.add("planner.solve_time", solve_time);
        stats.add("planner.total_time", step_time);

        // Print timestep solving metrics in structured format
        double current_memory = MemoryTracker::instance().get_current_memory_mb();

        Logger::instance().timestep_solving(VerbosityLevel::INFO, timestep, {
            {"formula", std::to_string(formula_time) + "s"},
            {"solve", std::to_string(solve_time) + "s"},
            {"step", std::to_string(step_time) + "s"},
            {"mem", std::to_string(static_cast<int>(current_memory)) + "MB"}
        });
        
        if (result == z3::sat) {
            Logger::instance().info("\n*** PLAN FOUND at timestep " + std::to_string(timestep) +
                                   " (total time: " + std::to_string(total_time) + "s) ***");

            // Mark that we found a solution
            solution_found_ = true;

            // Get and output the model to a file
            z3::model model = solver_.get_model();
            /*std::ofstream model_file("plan_model.txt");
            if (model_file.is_open()) {
                model_file << "Plan found at timestep " << timestep << std::endl;
                model_file << model << std::endl;
                model_file.close();
                Logger::instance().info("Model saved to plan_model.txt");
            }*/

            try {
                // Extract plan from model using encoder
                Plan plan = encoder_.extract_plan(model, timestep);
                //if (config.is_debug()) {
                //    Logger::instance().debug(plan.to_string());
                //}

                // Record successful solve
                stats.set("planner.plan_length", plan.length());
                stats.set("planner.solution_timestep", timestep);
                collect_statistics();

                // Clean up propagator before returning
                propagator_strategy_->cleanup();
                return plan; // Return the extracted plan

            } catch (const std::exception& e) {
                Logger::instance().error("ERROR during plan extraction: " + std::string(e.what()));
                Logger::instance().info("Returning empty plan.");

                // Collect statistics even on error
                collect_statistics();
                propagator_strategy_->cleanup();
                return Plan(); // Return empty plan on error
            }
            
        } else if (result == z3::unsat) {
            // let's try next iteration
        } else {
            Logger::instance().info("Solver returned unknown result at timestep " + std::to_string(timestep));
        }
    }

    Logger::instance().info("\n*** NO PLAN FOUND within " + std::to_string(config.planner.max_steps) + " timesteps, aborting ***");
    Logger::instance().info("No plan found within " + std::to_string(config.planner.max_steps) + " timesteps.");
    
    // Collect statistics after unsuccessful search
    collect_statistics();
    
    // Clean up propagator before returning
    propagator_strategy_->cleanup();
    return Plan(); // Return empty plan
}


void SequentialPlanner::set_propagator_strategy(std::unique_ptr<PropagatorStrategy> propagator) {
    propagator_strategy_ = std::move(propagator);
}

std::string SequentialPlanner::get_propagator_strategy_name() const {
    return propagator_strategy_->get_name();
}

} // namespace planmt
