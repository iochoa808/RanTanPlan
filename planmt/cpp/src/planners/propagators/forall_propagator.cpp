#include "forall_propagator.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analyzer.h"
#include <iostream>
#include <set>
#include <algorithm>

namespace planmt {

ForallPropagator::ForallPropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(nullptr),
     variable_factory_(nullptr), consistent_(true) {
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
                // We don't want to clean up empty timestep entries
                // as most probably they will be reused in the future
                
                trail_.pop_back();
            }
        }
    }
    consistent_ = true; // Reset consistency flag on pop
}

void ForallPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    if (!value.is_true() && consistent_) {
        // Only process true assignments and if propagator is consistent.
        // Note that we can be in a situation where the propagator is inconsistent
        // due to a previous list of propagations where we identified a conflict
        // at the start but we are still processing fixed callbacks.
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
    trail_.push_back({ast, true, timestep, action});
    
    // Update active actions for this timestep
    active_actions_per_timestep_[timestep].insert(action);
    
    // Perform forall propagation logic
    perform_forall_propagation(action, timestep, ast);  
}

void ForallPropagator::perform_forall_propagation(const Action& action, int timestep, const z3::expr& action_var) {

    // Get all actions that need to be negated when this action is true
    // (includes both incoming and outgoing interference edges)
    auto it = actions_interfering_with_.find(action);
    if (it == actions_interfering_with_.end()) { return; } // No actions interfere with this one
    const std::set<Action>& interfering_actions = it->second;
    
    // Get currently active actions at this timestep (empty set if none)
    const std::set<Action>& active_actions = active_actions_per_timestep_[timestep];
    
    // Find first conflict using early-terminating intersection
    Action first_conflict;
    bool has_conflict = false;
    
    // Manual intersection with early termination
    auto it1 = interfering_actions.begin();
    auto it2 = active_actions.begin();
    while (it1 != interfering_actions.end() && it2 != active_actions.end()) {
        if (*it1 < *it2) { ++it1;
        } else if (*it2 < *it1) { ++it2;
        } else { // Found conflict - stop immediately
            first_conflict = *it1;
            has_conflict = true;
            break;
        }
    }
    
    // If there's a conflict, report it and return
    if (has_conflict) {
        z3::expr interfering_var = variable_factory_->get_action_variable(first_conflict, timestep);
        z3::expr_vector conflict_vec(action_var.ctx());
        conflict_vec.push_back(action_var);
        conflict_vec.push_back(interfering_var);
        conflict(conflict_vec);
        consistent_ = false;
        return;
    }
    
    // We have no conflicts, so we can proceed with propagation!
    // Find actions to negate using set difference: interfering actions that are NOT active
    std::set<Action> actions_to_negate;
    std::set_difference(interfering_actions.begin(), interfering_actions.end(),
                        active_actions.begin(), active_actions.end(),
                        std::inserter(actions_to_negate, actions_to_negate.begin()));
    
    // Collect all negations to propagate in one batch
    z3::expr_vector negations_to_propagate(action_var.ctx());
    for (const Action& action_to_negate : actions_to_negate) {
        z3::expr interfering_var = variable_factory_->get_action_variable(action_to_negate, timestep);
        z3::expr negated_var = !interfering_var;
        negations_to_propagate.push_back(negated_var);
    }
        
    // Propagate all negations at once with current action as justification
    if (!negations_to_propagate.empty()) {
        // Create justification for the propagated negations
        z3::expr_vector justification(action_var.ctx());
        justification.push_back(action_var);
        // create consequence
        z3::expr all_negations = mk_and(negations_to_propagate);
        // and propagate the big and of all negations
        propagate(justification, all_negations);
        // Print the propagated clause
        //std::cout << "Propagated clause: " << all_negations.to_string() << " with justification: " << action_var.to_string() << std::endl;
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
    
    // Cache variable factory reference to avoid repeated lookups
    variable_factory_ = &encoder.get_variable_factory();
    
    // Set Z3 option to persist clauses for user propagator
    solver.set("smt.up.persist_clauses", true);
    
    // Build reverse interference lookup for efficient propagation
    build_reverse_interference_lookup();
}

void ForallPropagator::register_timestep_variables(int timestep) {
    
    const Z3VariableFactory& var_factory = *variable_factory_;
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

PropagatorType ForallPropagator::get_type() const {
    return PropagatorType::FORALL;
}

void ForallPropagator::build_reverse_interference_lookup() {
    const ParallelismStrategy* strategy = encoder_->get_parallelism_strategy();
    if (!strategy) return;
    
    const InterferenceAnalyzer* analyzer = strategy->get_interference_analyzer();
    if (!analyzer) return;
    
    // Clear any existing data
    actions_interfering_with_.clear();
    
    // Build complete interference lookup: for each action, find all actions that need to be negated
    // This includes both incoming edges (actions that interfere with this action) 
    // and outgoing edges (actions that this action interferes with)
    for (const Action& action : problem_->actions()) {
        Graph::NodeId node_id = analyzer->get_action_node_id(action);
        if (node_id < 0) continue;
        
        // Get all actions that 'action' interferes with (outgoing edges)
        const std::vector<Graph::NodeId>& interfered_with = 
            analyzer->get_interference_graph().get_neighbours(node_id);
        
        // Add outgoing edges: when 'action' is true, all actions it interferes with must be false
        for (Graph::NodeId target_node : interfered_with) {
            const Action* target_action = analyzer->get_action_from_node_id(target_node);
            if (target_action) {
                actions_interfering_with_[action].insert(*target_action);
            }
        }
        
        // Add incoming edges: when 'action' is true, all actions that interfere with it must be false
        // (This builds the reverse lookup from the outgoing edges of other actions)
        for (Graph::NodeId target_node : interfered_with) {
            const Action* target_action = analyzer->get_action_from_node_id(target_node);
            if (target_action) {
                actions_interfering_with_[*target_action].insert(action);
            }
        }
    }
    
    std::cout << "Built reverse interference lookup for " 
              << actions_interfering_with_.size() << " actions" << std::endl;
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
