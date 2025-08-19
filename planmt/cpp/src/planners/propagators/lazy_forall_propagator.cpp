#include "lazy_forall_propagator.h"
#include "../../config/config.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analysis.h"
#include <iostream>

namespace planmt {

LazyForallPropagator::LazyForallPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(&encoder),
     variable_factory_(&encoder.get_variable_factory()),
     parallelism_strategy_(encoder.get_parallelism_strategy()),
     interference_analyzer_(parallelism_strategy_->get_interference_analyzer()), conflict_count_(0) {
    // Define callbacks for the user propagator
    register_fixed();
    register_final();
    
    // Set Z3 option to persist clauses for user propagator based on config
    solver.set("smt.up.persist_clauses", Config::instance().propagators.persist_clauses);
    
    // Clear any existing state
    active_actions_per_timestep_.clear();
}

void LazyForallPropagator::push() {
    // Z3 is entering a new backtracking scope - mark decision level
    decision_levels_.push_back(trail_.size());
}

void LazyForallPropagator::pop(unsigned num_scopes) {
    // Z3 is backtracking - undo changes for each scope
    for (unsigned i = 0; i < num_scopes; ++i) {
        // Find the start of the current decision level
        size_t level_start = decision_levels_.back();
        decision_levels_.pop_back();
        
        // Undo all trail entries added after this level
        while (trail_.size() > level_start) {
            const auto& [action_node_id, timestep] = trail_.back();
            
            // Remove action from active set using NodeId
            auto& active_set = active_actions_per_timestep_[timestep];
            active_set.erase(action_node_id);
            
            trail_.pop_back();
        }
    }
}

void LazyForallPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    if (!value.is_true()) return; // Only process true assignments
    
    // Extract action and timestep from the variable
    auto action_info = variable_factory_->get_action_from_variable(ast);
    const Action& action = action_info->first;
    int timestep = action_info->second;
    
    // Get NodeId for the action
    Graph::NodeId action_node_id = interference_analyzer_->get_action_node_id(action);
    
    // Add to trail using NodeId and timestep
    trail_.push_back({action_node_id, timestep});
    
    // Update active actions for this timestep using NodeId
    active_actions_per_timestep_[timestep].insert(action_node_id);
    
    // Check for conflicts with other active actions at the same timestep
    check_and_generate_conflicts(action, timestep, ast);  
}

void LazyForallPropagator::check_and_generate_conflicts(const Action& action, int timestep, const z3::expr& action_var) {
    // Get the set of active NodeIds at this timestep
    Graph::NodeId current_node_id = interference_analyzer_->get_action_node_id(action);
    
    // Check for interference with each other active action at this timestep
    z3::expr_vector conflicting_actions(ctx());
    for (Graph::NodeId other_node_id : active_actions_per_timestep_[timestep]) {
        if (other_node_id == current_node_id) continue; // Skip self
        
        // Check if there's interference in either direction using optimized NodeId method
        if (interference_analyzer_->has_interference(current_node_id, other_node_id) || 
            interference_analyzer_->has_interference(other_node_id, current_node_id)) {
            
            // Convert NodeId back to Action to get the variable
            const Action* other_action = interference_analyzer_->get_action_from_node_id(other_node_id);
            z3::expr other_var = variable_factory_->get_action_variable(*other_action, timestep);
            conflicting_actions.push_back(other_var);
        }
    }
    
    // If we found any conflicts, generate a conflict clause
    if (!conflicting_actions.empty()) {
        // In forall semantics, we should typically only have pairwise conflicts
        assert(conflicting_actions.size() == 1 && "Expected only pairwise conflicts in forall propagator");
        
        // Create justification: all the conflicting actions plus the current action
        z3::expr_vector justification(action_var.ctx());
        justification.push_back(action_var);
        for (const z3::expr& conflicting_var : conflicting_actions) {
            justification.push_back(conflicting_var);
        }
        
        // Increment conflict counter
        conflict_count_++;
        
        // Generate conflict: these actions cannot all be true simultaneously
        conflict(justification);
    }
}

void LazyForallPropagator::final() {
    // TODO: we need this?
    
}

z3::user_propagator_base* LazyForallPropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    // TODO: Implement proper fresh instance creation if needed
    return nullptr;
}

void LazyForallPropagator::register_timestep_variables(int timestep) {
    const Z3VariableFactory& var_factory = *variable_factory_;
    // For timestep 0: register nothing as there are no actions
    if (timestep == 0) return;
    
    // For timestep t > 0: register action variables for t-1 
    if (!registered_action_vars_.contains(timestep - 1)) {
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
    // Print conflict count statistics when propagator is destroyed
    std::cout << "LazyForallPropagator threw " << conflict_count_ << " conflicts" << std::endl;
    
    // Clear all state
    active_actions_per_timestep_.clear();
    trail_.clear();
    decision_levels_.clear();
    registered_action_vars_.clear();
}


} // namespace planmt