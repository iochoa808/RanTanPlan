#include "forall_propagator.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analyzer.h"
#include <iostream>

namespace planmt {

ForallPropagator::ForallPropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(nullptr) {
    
    // Define callbacks for the user propagator
    register_fixed();
}

void ForallPropagator::push() {
    // Z3 is entering a new backtracking scope - mark decision level
    decision_levels_.push_back(trail_.size());
}

void ForallPropagator::pop(unsigned num_scopes) {
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
                
                // Clean up empty timestep entries
                if (active_set.empty()) {
                    active_actions_per_timestep_.erase(entry.timestep);
                    propagated_pairs_.erase(entry.timestep);
                }
                
                trail_.pop_back();
            }
        }
    }
    
    // Print trail state after backtracking
    //print_trail_state();
}

void ForallPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    // Extract action and timestep from the variable
    auto action_info = encoder_->get_variable_factory().get_action_from_variable(ast);
    if (!action_info || !value.is_true()) {
        return; // Only process true action assignments
    }
    
    const Action& action = action_info->first;
    int timestep = action_info->second;
    
    // Add to trail
    trail_.push_back({ast, true, timestep, action});
    
    // Update active actions for this timestep
    active_actions_per_timestep_[timestep].insert(action);
    
    // Perform forall propagation logic
    perform_forall_propagation(action, timestep, ast);  
}

void ForallPropagator::perform_forall_propagation(const Action& action, int timestep, const z3::expr& action_var) {
    // Get interference analyzer from the parallelism strategy
    const ParallelismStrategy* strategy = encoder_->get_parallelism_strategy();
    if (!strategy) return;
    
    const InterferenceAnalyzer* analyzer = strategy->get_interference_analyzer();
    if (!analyzer) return;
    
    // Check for conflicts with currently active actions at this timestep
    std::vector<z3::expr> conflict_literals;
    
    for (const Action& active_action : active_actions_per_timestep_[timestep]) {
        if (active_action == action) continue; // Skip self
        
        if (analyzer->has_interference(action, active_action)) {
            // Conflict detected - collect the conflicting action variable
            z3::expr active_var = encoder_->get_variable_factory().get_action_variable(active_action, timestep);
            conflict_literals.push_back(active_var);
        }
    }
    
    // If conflicts found, report to Z3
    if (!conflict_literals.empty()) {
        conflict_literals.push_back(action_var); // Add current action to conflict
        
        // Convert to z3::expr_vector for conflict API
        z3::expr_vector conflict_vec(action_var.ctx());
        for (const auto& lit : conflict_literals) {
            conflict_vec.push_back(lit);
        }
        conflict(conflict_vec); // Report conflict to Z3
        return;
    }
    
    // Propagate negations for interfering actions not yet active
    // Iterate through all actions in the problem to find interfering ones
    for (const auto& other_action : problem_->actions()) {
        if (other_action == action) continue; // Skip self
        
        // Check if this action interferes with the newly fixed action
        if (analyzer->has_interference(action, other_action)) {
            // Check if already active at this timestep
            if (active_actions_per_timestep_[timestep].count(other_action) > 0) {
                continue; // Already handled in conflict detection above
            }
            
            // Check if we've already propagated this pair
            if (has_propagated_pair(action, other_action, timestep)) {
                continue; // Already propagated
            }
            
            // Propagate negation of the interfering action
            z3::expr other_action_var = encoder_->get_variable_factory().get_action_variable(other_action, timestep);
            z3::expr negated_var = !other_action_var;
            
            // Propagate with current action as justification
            z3::expr_vector justification(action_var.ctx());
            justification.push_back(action_var);
            propagate(justification, negated_var);
            
            // Mark this pair as propagated
            mark_propagated_pair(action, other_action, timestep);
        }
    }
}

z3::user_propagator_base* ForallPropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    // TODO: Implement proper fresh instance creation if needed
    return nullptr;
}

void ForallPropagator::initialize(z3::solver& solver, const GroundedEncoder& encoder) {
    // Store reference to encoder for variable factory access
    encoder_ = &encoder;
}

void ForallPropagator::register_timestep_variables(int timestep) {
    
    const Z3VariableFactory& var_factory = encoder_->get_variable_factory();
    // For timestep 0: register nothing as there are no actions
    if (timestep == 0) return;
    // For timestep t > 0: register action variables for t-1 
    // 1. Register action variables for timestep t-1
    if (registered_action_vars_.find(timestep - 1) == registered_action_vars_.end()) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            for (const auto& var : registered_action_vars_[timestep - 1]) {
                add(var); //std::cout << "  Registered with Z3: " << var.to_string() << std::endl;
            }
        }
    }
}

bool ForallPropagator::has_propagated_pair(const Action& a1, const Action& a2, int timestep) const {
    auto it = propagated_pairs_.find(timestep);
    if (it == propagated_pairs_.end()) return false;
    
    const auto& pairs = it->second;
    return pairs.count({a1, a2}) > 0 || pairs.count({a2, a1}) > 0;
}

void ForallPropagator::mark_propagated_pair(const Action& a1, const Action& a2, int timestep) {
    propagated_pairs_[timestep].insert({a1, a2});
    propagated_pairs_[timestep].insert({a2, a1}); // Mark both directions
}

PropagatorType ForallPropagator::get_type() const {
    return PropagatorType::FORALL;
}

void ForallPropagator::print_trail_state() const {
    std::cout << "Trail: [";
    for (size_t i = 0; i < trail_.size(); ++i) {
        if (i > 0) std::cout << ", ";
        
        const TrailEntry& entry = trail_[i];
        // Determine if this is a decision or propagation by checking if it's at a decision level start
        bool is_decision = false;
        for (size_t level : decision_levels_) {
            if (i == level) {
                is_decision = true;
                break;
            }
        }
        
        std::cout << "'" << (is_decision ? "D" : "P") << ": " 
                  << entry.action.name() << "_t" << entry.timestep 
                  << "=" << (entry.value ? "T" : "F") << "'";
    }
    std::cout << "]" << std::endl;
    
    std::cout << "Decision Levels: [";
    for (size_t i = 0; i < decision_levels_.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << decision_levels_[i];
    }
    std::cout << "]" << std::endl;
}

} // namespace planmt
