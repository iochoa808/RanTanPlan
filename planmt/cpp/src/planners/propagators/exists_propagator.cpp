#include "exists_propagator.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analyzer.h"
#include <iostream>
#include <set>
#include <algorithm>

namespace planmt {

ExistsPropagator::ExistsPropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(nullptr),
     variable_factory_(nullptr) {
    // Define callbacks for the user propagator
    register_fixed();
}

void ExistsPropagator::push() {
    // Z3 is entering a new backtracking scope - mark decision level
    decision_levels_.push_back(trail_.size());
}

void ExistsPropagator::pop(unsigned num_scopes) {
    // Z3 is backtracking - undo changes for each scope
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (!decision_levels_.empty()) {
            // Find the start of the current decision level
            size_t level_start = decision_levels_.back();
            decision_levels_.pop_back();
            
            // Undo all trail entries added after this level
            while (trail_.size() > level_start) {
                const TrailEntry& entry = trail_.back();
                
                // Remove action from active set
                auto& active_set = active_actions_per_timestep_[entry.timestep];
                active_set.erase(entry.action);
                
                // No ordering graph to clean up
                
                trail_.pop_back();
            }
        }
    }
}

void ExistsPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    if (!value.is_true()) {
        // Only process true assignments
        return;
    }
    
    // Extract action and timestep from the variable
    auto action_info = variable_factory_->get_action_from_variable(ast);
    if (!action_info) {
        return; // Only process true action assignments
    }
    
    const Action& action = action_info->first;
    int timestep = action_info->second;
    
    // Add to trail
    trail_.push_back({ast, timestep, action});
    
    // Update active actions for this timestep
    active_actions_per_timestep_[timestep].insert(action);
    
    // Perform exists propagation logic
    perform_exists_propagation(action, timestep, ast);
}

z3::user_propagator_base* ExistsPropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    return nullptr;
}

void ExistsPropagator::initialize(z3::solver& solver, const GroundedEncoder& encoder) {
    // Store reference to encoder for variable factory access
    encoder_ = &encoder;
    
    // Cache variable factory reference to avoid repeated lookups
    variable_factory_ = &encoder.get_variable_factory();
    
    // Set Z3 option to persist clauses for user propagator
    solver.set("smt.up.persist_clauses", true);
    
    // Build interference lookup for efficient propagation
    build_interference_lookup();
}

void ExistsPropagator::register_timestep_variables(int timestep) {
    const Z3VariableFactory& var_factory = *variable_factory_;
    // For timestep 0: register nothing as there are no actions
    if (timestep == 0) return;
    
    // For timestep t > 0: register action variables for t-1 
    if (registered_action_vars_.find(timestep - 1) == registered_action_vars_.end()) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            for (const auto& var : registered_action_vars_[timestep - 1]) {
                add(var);
            }
        }
    }
}

PropagatorType ExistsPropagator::get_type() const {
    return PropagatorType::EXISTS;
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
    
    // Get currently active actions at this timestep (including the current action)
    const std::set<Action>& active_actions = active_actions_per_timestep_[timestep];
    
    // Build interference graph for active actions only (local variable)
    std::unordered_map<Action, std::unordered_set<Action>> successors;
    for (const Action& active_action : active_actions) {
        auto it = interference_neighbours_.find(active_action);
        if (it != interference_neighbours_.end()) {
            for (const Action& neighbour : it->second) {
                // Only add edges between active actions
                if (active_actions.count(neighbour) > 0) {
                    successors[active_action].insert(neighbour);
                }
            }
        }
    }
    
    // Check if there's a cycle among the active actions
    std::vector<Action> cycle;
    if (find_cycle_among_active_actions(active_actions, successors, cycle)) {
        // Report conflict with all actions in the cycle
        z3::expr_vector conflict_actions(action_var.ctx());
        for (const Action& cycle_action : cycle) {
            z3::expr cycle_var = variable_factory_->get_action_variable(cycle_action, timestep);
            conflict_actions.push_back(cycle_var);
        }
        conflict(conflict_actions);
    }
}

bool ExistsPropagator::find_cycle_among_active_actions(const std::set<Action>& active_actions, 
                                                     const std::unordered_map<Action, std::unordered_set<Action>>& successors,
                                                     std::vector<Action>& cycle) {
    // Try to find a cycle starting from each active action
    for (const Action& start_action : active_actions) {
        std::unordered_set<Action> visited;
        std::vector<Action> path;
        
        if (find_cycle_dfs(start_action, start_action, active_actions, successors, visited, path)) {
            cycle = path;
            return true;
        }
    }
    return false;
}

bool ExistsPropagator::find_cycle_dfs(const Action& current, const Action& target, 
                                     const std::set<Action>& active_actions,
                                     const std::unordered_map<Action, std::unordered_set<Action>>& successors,
                                     std::unordered_set<Action>& visited, 
                                     std::vector<Action>& path) {
    path.push_back(current);
    visited.insert(current);
    
    // Check successors
    auto it = successors.find(current);
    if (it != successors.end()) {
        for (const Action& successor : it->second) {
            // Only consider active actions
            if (active_actions.count(successor) == 0) continue;
            
            if (successor == target && path.size() > 1) {
                // Found cycle back to target
                return true;
            }
            
            if (visited.count(successor) == 0) {
                if (find_cycle_dfs(successor, target, active_actions, successors, visited, path)) {
                    return true;
                }
            }
        }
    }
    
    // Backtrack
    path.pop_back();
    return false;
}

void ExistsPropagator::build_interference_lookup() {
    const ParallelismStrategy* strategy = encoder_->get_parallelism_strategy();
    if (!strategy) return;
    
    const InterferenceAnalyzer* analyzer = strategy->get_interference_analyzer();
    if (!analyzer) return;
    
    // Clear any existing data
    interference_neighbours_.clear();
    
    // Build interference lookup: for each action, find all actions that interfere with it
    for (const Action& action : problem_->actions()) {
        Graph::NodeId node_id = analyzer->get_action_node_id(action);
        if (node_id < 0) continue;
        
        // Get all actions that this action interferes with (outgoing edges)
        const std::vector<Graph::NodeId>& interfered_with = 
            analyzer->get_interference_graph().get_neighbours(node_id);
        
        // Add to interference neighbours
        for (Graph::NodeId target_node : interfered_with) {
            const Action* target_action = analyzer->get_action_from_node_id(target_node);
            if (target_action) {
                interference_neighbours_[action].insert(*target_action);
                // Also add reverse lookup for symmetric interference
                interference_neighbours_[*target_action].insert(action);
            }
        }
    }
    
    std::cout << "Built interference lookup for " 
              << interference_neighbours_.size() << " actions" << std::endl;
}

} // namespace planmt