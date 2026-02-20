#include "exists_propagator.hpp"
#include "../../config/config.hpp"
#include "../../util/memory_tracker.hpp"
#include "../../util/stats.hpp"
#include "../../encoders/z3_variable_factory.hpp"
#include "../../encoders/parallelism/interference_analysis.hpp"
#include <iostream>
#include <set>
#include <algorithm>
#include <functional>

namespace rantanplan {

ExistsPropagator::ExistsPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder)
    : PropagatorStrategy(solver, encoder), problem_(&problem),
     variable_factory_(&encoder.get_variable_factory()),
     parallelism_strategy_(encoder.get_parallelism_strategy()),
     interference_analyzer_(parallelism_strategy_->get_interference_analyzer()), cycle_count_(0) {
    // Set Z3 option to persist clauses for user propagator based on config
    solver.set("smt.up.persist_clauses", Config::instance().global.persist_clauses);
}

void ExistsPropagator::on_push() {
    // Z3 is entering a new backtracking scope - mark decision level
    decision_levels_.push_back(trail_.size());
}

void ExistsPropagator::on_pop(unsigned num_scopes) {
    // Z3 is backtracking - undo changes for each scope
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (!decision_levels_.empty()) {
            // Find the start of the current decision level
            size_t level_start = decision_levels_.back();
            decision_levels_.pop_back();
            
            // Undo all trail entries added after this level
            while (trail_.size() > level_start) {
                const auto& [action_node_id, timestep] = trail_.back();
                
                // Remove action from active set using int
                auto& active_set = active_actions_per_timestep_[timestep];
                active_set.erase(action_node_id);
                
                trail_.pop_back();
            }
        }
    }
}

void ExistsPropagator::on_fixed(z3::expr const &ast, z3::expr const &value) {
    if (!value.is_true()) return; // Only process true assignments

    // Extract action and timestep from the variable
    auto action_info = variable_factory_->get_action_from_variable(ast);
    if (!action_info) return; // Not an action variable (e.g., fluent registered for logging)
    const Action& action = action_info->first;
    int timestep = action_info->second;
    
    // Get int for the action
    int action_node_id = action.id();
    
    // Add to trail using int and timestep
    trail_.push_back({action_node_id, timestep});
    
    // Update active actions for this timestep using int
    active_actions_per_timestep_[timestep].insert(action_node_id);
    
    // Perform exists propagation logic
    perform_exists_propagation(action, timestep, ast);
}

void ExistsPropagator::register_timestep_variables(int timestep) {
    // Base class handles logging (inc, variable registration)
    PropagatorStrategy::register_timestep_variables(timestep);
    const Z3VariableFactory& var_factory = *variable_factory_;
    // For timestep 0: register nothing as there are no actions
    if (timestep == 0) return;
    
    // For timestep t > 0: register action variables for t-1 
    if (!registered_action_vars_.contains(timestep - 1)) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            for (const auto& var_ptr : registered_action_vars_[timestep - 1]) {
                add(*var_ptr);
            }
        }
    }
}

void ExistsPropagator::cleanup() {
    auto& stats = Stats::instance();
    stats.set("propagator.exists_total_cycles", cycle_count_);
}

void ExistsPropagator::perform_exists_propagation(const Action& action, int timestep, const z3::expr& action_var) {
    /*
     * EXAMPLE: Cycle detection with EXISTS semantics (3-action cycle)
     * 
     * Say we have actions at timestep 1 that form a cycle:
     *   - action_A (interferes with action_B)
     *   - action_B (interferes with action_C)  
     *   - action_C (interferes with action_A)
     * 
     * The interference pattern creates a cycle: A → B → C → A
     * 
     * When all three actions become active, we detect the cycle and report 
     * conflict with ALL actions in the cycle: {action_A, action_B, action_C}
     */
    
    // Get currently active action node IDs at this timestep (including the current action)
    const std::unordered_set<int>& active_node_ids = active_actions_per_timestep_[timestep];
    
    // Check if there's a cycle among the active actions
    std::vector<int> cycle;
    if (find_cycle_in_active_actions(active_node_ids, cycle)) {
        // Increment cycle counter
        cycle_count_++;
        
        // Report conflict with all actions in the cycle
        z3::expr_vector conflict_actions(action_var.ctx());
        for (int cycle_node_id : cycle) {
            // Convert int back to Action to get the variable
            const Action* cycle_action = &problem_->action(cycle_node_id);
            z3::expr cycle_var = variable_factory_->get_action_variable(*cycle_action, timestep);
            conflict_actions.push_back(cycle_var);
        }
        conflict(conflict_actions);
    }
}

bool ExistsPropagator::find_cycle_in_active_actions(const std::unordered_set<int>& active_node_ids, 
                                                   std::vector<int>& cycle) {
    if (active_node_ids.size() < 2) return false;
    
    std::unordered_set<int> visited;
    std::unordered_set<int> recursion_stack;
    std::vector<int> path;
    
    // Lambda for DFS with inline graph building
    std::function<bool(int)> dfs = [&](int current) -> bool {
        visited.insert(current);
        recursion_stack.insert(current);
        path.push_back(current);
        
        // Check interference with other active nodes
        for (int other_node : active_node_ids) {
            if (other_node == current) continue;
            
            if (interference_analyzer_->has_interference(current, other_node)) {
                if (recursion_stack.contains(other_node)) {
                    // Found back edge - cycle detected
                    auto cycle_start = std::find(path.begin(), path.end(), other_node);
                    cycle.assign(cycle_start, path.end());
                    return true;
                }
                
                if (!visited.contains(other_node)) {
                    if (dfs(other_node)) {
                        return true;
                    }
                }
            }
        }
        
        // Backtrack
        recursion_stack.erase(current);
        path.pop_back();
        return false;
    };
    
    // Try to find cycle starting from each unvisited node
    for (int start_node : active_node_ids) {
        if (!visited.count(start_node)) {
            if (dfs(start_node)) {
                return true;
            }
        }
    }
    
    return false;
}

} // namespace rantanplan