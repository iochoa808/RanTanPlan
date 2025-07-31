#include "sequential.h"
#include "../config/config.h"
#include "../encoders/parallelism/graph.h"
#include "../encoders/parallelism/interference_analyzer.h"
#include "propagators/null_propagator.h"
#include "propagators/propagator_factory.h"
#include "../util/memory_tracker.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <unordered_map>

namespace planmt {

    SequentialPlanner::SequentialPlanner(const Problem& problem, GroundedEncoder& encoder, z3::context& ctx) 
        : problem_(problem), encoder_(encoder), ctx_(ctx), solver_(ctx),
          propagator_strategy_(std::make_unique<NullPropagator>()) {
        // Initialize planner with the given problem, encoder, and Z3 context
        // Uses null propagator by default (no propagation)
    }

    SequentialPlanner::SequentialPlanner(const Problem& problem, GroundedEncoder& encoder, z3::context& ctx, 
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
        std::cerr << "Error: Could not open output.smt2 for writing" << std::endl;
    }
}

Plan SequentialPlanner::search() {
    auto& config = Config::instance();
    
    if (config.is_info()) {
        std::cout << "Starting search with propagator: " << propagator_strategy_->get_name() << std::endl;
    }
    
    // Initialize propagator once at the beginning
    propagator_strategy_->initialize(solver_, encoder_);
    
    // Reset solution found flag
    solution_found_ = false;

    // Add initial state constraints (these are invariant)
    solver_.add(*encoder_.encode_initial_state());
    
    // Register variables for timestep 0 (initial state)
    propagator_strategy_->register_timestep_variables(0);
    
    // Iterate from 0 to max_steps timesteps
    double total_time = 0.0; // Track total time used
    auto start_time = std::chrono::high_resolution_clock::now(); // Track total search time for timeout
    
    for (int timestep = 0; timestep <= config.planner.max_steps; ++timestep) {
        // Check timeout
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration<double>(current_time - start_time).count();
        if (elapsed_seconds >= config.global.timeout) {
            if (config.is_info()) {
                std::cout << "\n*** TIMEOUT reached after " << elapsed_seconds << "s ***" << std::endl;
            }
            break;
        }
        auto step_start = std::chrono::high_resolution_clock::now();
        
        // Time formula creation
        auto formula_start = std::chrono::high_resolution_clock::now();
        
        // Add constraints for this timestep (incremental)
        if (timestep > 0) {
            solver_.add(*encoder_.encode_actions(timestep-1));
            solver_.add(*encoder_.encode_frames(timestep-1));
            
            // Only add parallelism constraints if not using ForallPropagator
            // (ForallPropagator handles interference dynamically via propagation)
            if (get_propagator_type() != PropagatorType::FORALL
                && get_propagator_type() != PropagatorType::EXISTS) {
                solver_.add(*encoder_.encode_parallelism(timestep-1));
            }
            
            // Register variables for timestep after constraints are added
            propagator_strategy_->register_timestep_variables(timestep);
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

        if (config.is_info()) {
            std::cout << "T" << timestep;
        }
        //debug_output_constraints(); // Output initial constraints

        // Time solving
        auto solve_start = std::chrono::high_resolution_clock::now();
        z3::check_result result = solver_.check(assumptions);
        auto solve_end = std::chrono::high_resolution_clock::now();
        auto solve_time = std::chrono::duration<double>(solve_end - solve_start).count();
        
        auto step_end = std::chrono::high_resolution_clock::now();
        auto step_time = std::chrono::duration<double>(step_end - step_start).count();

        total_time += step_time; // Accumulate total time
        
        // Print timing in compact format at INFO level
        if (config.is_info()) {
            double current_memory = MemoryTracker::instance().get_current_memory_mb();
            std::cout << " timing: formula=" << formula_time << "s, solve=" << solve_time << "s, step=" << step_time << "s";
            std::cout << ", memory=" << current_memory << "MB";
            std::cout << std::endl;
        }
        
        if (result == z3::sat) {
            if (config.is_info()) {
                std::cout << "\n*** PLAN FOUND at timestep " << timestep << " (total time: " << total_time << "s) ***" << std::endl;
            }
            
            // Mark that we found a solution
            solution_found_ = true;
            
            // Get and output the model to a file
            z3::model model = solver_.get_model();
            /*std::ofstream model_file("plan_model.txt");
            if (model_file.is_open()) {
                model_file << "Plan found at timestep " << timestep << std::endl;
                model_file << model << std::endl;
                model_file.close();
                std::cout << "Model saved to plan_model.txt" << std::endl;
            }*/
            
            try {
                // Extract plan from model
                Plan plan = extract_plan(model, timestep);
                if (config.is_info()) {
                    std::cout << plan.to_string() << std::endl;
                }
                
                // Clean up propagator before returning
                propagator_strategy_->cleanup();
                return plan; // Return the extracted plan
                
            } catch (const std::exception& e) {
                if (config.is_info()) {
                    std::cout << "ERROR during plan extraction: " << e.what() << std::endl;
                }
                if (config.is_info()) {
                    std::cout << "Returning empty plan." << std::endl;
                }
                propagator_strategy_->cleanup();
                return Plan(); // Return empty plan on error
            }
            
        } else if (result == z3::unsat) {
            // let's try next iteration
        } else {
            if (config.is_info()) {
                std::cout << "Solver returned unknown result at timestep " << timestep << std::endl;
            }
        }
    }
    
    if (config.is_info()) {
        std::cout << "\n*** NO PLAN FOUND within " << config.planner.max_steps << " timesteps, aborting ***" << std::endl;
        std::cout << "No plan found within " << config.planner.max_steps << " timesteps." << std::endl;
    }
    
    // Clean up propagator before returning
    propagator_strategy_->cleanup();
    return Plan(); // Return empty plan
}

Plan SequentialPlanner::extract_plan(const z3::model& model, int max_timestep) {
    Plan plan;
    
    std::cout << "Extracting plan from Z3 model with " << model.size() << " variable assignments" << std::endl;
    
    std::string strategy_name = encoder_.get_parallelism_strategy_name();
    bool is_parallel = (strategy_name == "ForallSemantics" || strategy_name == "ExistsSemantics");
    
    // Iterate through each timestep
    for (int t = 0; t <= max_timestep; ++t) {
        if (is_parallel) {
            // Extract and order parallel actions for this timestep
            std::vector<const Action*> parallel_actions = extract_parallel_actions_at_timestep(model, t);
            
            if (!parallel_actions.empty()) {
                std::vector<const Action*> ordered_actions = topologically_sort_actions(parallel_actions);
                
                //std::cout << "Timestep " << t << ": " << ordered_actions.size() << " actions in topological order" << std::endl;
                
                // Add ordered actions to plan
                for (const Action* action : ordered_actions) {
                    //std::cout << "action: " << action->name() << std::endl;
                    plan.add_action(action);
                }
            }
        } else {
            // Original sequential extraction logic
            for (const Action& grounded_action : problem_.actions()) {
                try {
                    // Get the Z3 variable for this grounded action at this timestep
                    z3::expr action_var = encoder_.get_variable_factory().get_action_variable(grounded_action, t);
                    
                    // Evaluate the action variable in the model
                    z3::expr action_value = model.eval(action_var, true); // Use model completion
                    
                    // If this action is true in the model, add it to the plan
                    if (action_value.is_true()) {
                        plan.add_action(&grounded_action);
                        break; // Only one action in sequential mode
                    }
                } catch (const std::exception& e) {
                    std::cout << "  Error evaluating action " << grounded_action.name() << " at timestep " << t 
                              << ": " << e.what() << std::endl;
                }
            }
        }
    }
    return plan;
}

std::vector<const Action*> SequentialPlanner::extract_parallel_actions_at_timestep(
    const z3::model& model, int timestep) {
    
    std::vector<const Action*> parallel_actions;
    
    for (const Action& grounded_action : problem_.actions()) {
        try {
            z3::expr action_var = encoder_.get_variable_factory().get_action_variable(grounded_action, timestep);
            z3::expr action_value = model.eval(action_var, true);
            
            if (action_value.is_true()) {
                parallel_actions.push_back(&grounded_action);
            }
        } catch (const std::exception& e) {
            std::cout << "  Error evaluating action " << grounded_action.name() << " at timestep " << timestep 
                      << ": " << e.what() << std::endl;
        }
    }
    
    return parallel_actions;
}

std::vector<const Action*> SequentialPlanner::topologically_sort_actions(
    const std::vector<const Action*>& actions) {
    
    std::vector<const Action*> result;
    
    if (actions.size() <= 1) {
        result = actions; // No sorting needed
    } else {
        const ParallelismStrategy* strategy = encoder_.get_parallelism_strategy();
        if (!strategy) {
            result = actions; // No parallelism strategy available
        } else {
            const InterferenceAnalyzer* analyzer = strategy->get_interference_analyzer();
            if (!analyzer) {
                result = actions; // No interference analyzer available
            } else {
                // Let the InterferenceAnalyzer handle the topological sorting
                result = analyzer->topological_sort_actions(actions);
            }
        }
    }
    
    return result;
}

void SequentialPlanner::set_propagator_strategy(PropagatorType type) {
    propagator_strategy_ = PropagatorFactory::create_strategy(type, solver_, problem_);
}

void SequentialPlanner::set_propagator_strategy(const std::string& strategy_name) {
    propagator_strategy_ = PropagatorFactory::create_strategy(strategy_name, solver_, problem_);
}

std::string SequentialPlanner::get_propagator_strategy_name() const {
    return propagator_strategy_->get_name();
}

PropagatorType SequentialPlanner::get_propagator_type() const {
    return propagator_strategy_->get_type();
}

} // namespace planmt
