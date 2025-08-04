#include "lazy_forall_propagator.h"
#include "../../config/config.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analysis.h"
#include <iostream>

namespace planmt {

LazyForallPropagator::LazyForallPropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(nullptr),
     variable_factory_(nullptr) {
    // Define callbacks for the user propagator
    register_fixed();
}

void LazyForallPropagator::push() {
    // Z3 is entering a new backtracking scope - mark decision level
    decision_levels_.push_back(trail_.size());
}

void LazyForallPropagator::pop(unsigned num_scopes) {
    // Z3 is backtracking - undo changes for each scope
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (!decision_levels_.empty()) {
            // Find the start of the current decision level
            size_t level_start = decision_levels_.back();
            decision_levels_.pop_back();
            
            // Undo all trail entries added after this level
            while (trail_.size() > level_start) {
                const TrailEntry& entry = trail_.back();
                
                // Derive action and timestep from variable
                auto action_info = variable_factory_->get_action_from_variable(entry.variable);
                if (action_info) {
                    const Action& action = action_info->first;
                    int timestep = action_info->second;
                    
                    // Remove action from active set
                    auto& active_set = active_actions_per_timestep_[timestep];
                    active_set.erase(action);
                }
                
                trail_.pop_back();
            }
        }
    }
}

void LazyForallPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    if (!value.is_true()) {
        // Only process true assignments
        return;
    }
    
    // Extract action and timestep from the variable
    auto action_info = variable_factory_->get_action_from_variable(ast);
    if (!action_info) {
        return; // Only process action assignments
    }
    
    const Action& action = action_info->first;
    int timestep = action_info->second;
    
    // Add to trail
    trail_.push_back({ast});
    
    // Update active actions for this timestep
    active_actions_per_timestep_[timestep].insert(action);
    
    // Check for conflicts with other active actions at the same timestep
    check_and_generate_conflicts(action, timestep, ast);  
}

void LazyForallPropagator::check_and_generate_conflicts(const Action& action, int timestep, const z3::expr& action_var) {
    const InterferenceAnalysis* analyzer = get_interference_analyzer();
    if (!analyzer) {
        return; // No interference analyzer available
    }
    
    // Get the set of active actions at this timestep (excluding the current action)
    auto it = active_actions_per_timestep_.find(timestep);
    if (it == active_actions_per_timestep_.end()) {
        return; // No other active actions at this timestep
    }
    
    const std::unordered_set<Action>& active_actions = it->second;
    
    // Check for interference with each other active action at this timestep
    z3::expr_vector conflicting_actions(action_var.ctx());
    for (const Action& other_action : active_actions) {
        if (other_action == action) {
            continue; // Skip self
        }
        
        // Check if there's interference in either direction
        if (analyzer->has_interference(action, other_action) || 
            analyzer->has_interference(other_action, action)) {
            
            // Add the conflicting action variable to our conflict set
            z3::expr other_var = variable_factory_->get_action_variable(other_action, timestep);
            conflicting_actions.push_back(other_var);
        }
    }
    
    // If we found any conflicts, generate a conflict clause
    if (conflicting_actions.size() > 0) {
        // Create justification: all the conflicting actions plus the current action
        z3::expr_vector justification(action_var.ctx());
        justification.push_back(action_var);
        for (const z3::expr& conflicting_var : conflicting_actions) {
            justification.push_back(conflicting_var);
        }
        
        // Generate conflict: these actions cannot all be true simultaneously
        conflict(justification);
    }
}

z3::user_propagator_base* LazyForallPropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    // TODO: Implement proper fresh instance creation if needed
    return nullptr;
}

void LazyForallPropagator::initialize(z3::solver& solver, const GroundedEncoder& encoder) {
    // Store reference to encoder for variable factory access
    encoder_ = &encoder;
    
    // Cache variable factory reference to avoid repeated lookups
    variable_factory_ = &encoder.get_variable_factory();
    
    // Set Z3 option to persist clauses for user propagator based on config
    solver.set("smt.up.persist_clauses", Config::instance().propagators.persist_clauses);
    
    // Clear any existing state
    active_actions_per_timestep_.clear();
}

void LazyForallPropagator::register_timestep_variables(int timestep) {
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

PropagatorType LazyForallPropagator::get_type() const {
    return PropagatorType::LAZY_FORALL;
}

void LazyForallPropagator::cleanup() {
    // Clear all state
    active_actions_per_timestep_.clear();
    trail_.clear();
    decision_levels_.clear();
    registered_action_vars_.clear();
}

const InterferenceAnalysis* LazyForallPropagator::get_interference_analyzer() const {
    const ParallelismStrategy* strategy = encoder_->get_parallelism_strategy();
    if (!strategy) {
        return nullptr;
    }
    
    return strategy->get_interference_analyzer();
}

} // namespace planmt