#include "forall_propagator.h"
#include "../../config/config.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analysis.h"
#include "../../util/stats.h"
#include <iostream>
#include <set>
#include <cassert>

namespace planmt {

ForallPropagator::ForallPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(&encoder),
     variable_factory_(&encoder.get_variable_factory()),
     parallelism_strategy_(encoder.get_parallelism_strategy()),
     interference_analyzer_(parallelism_strategy_->get_interference_analyzer()), propagation_count_(0) {
    // Define callbacks for the user propagator
    register_fixed();
    register_final();
    
    // Set Z3 option to persist clauses for user propagator based on config
    solver.set("smt.up.persist_clauses", Config::instance().propagators.persist_clauses);
    
    // Build reverse interference lookup for efficient propagation
    build_reverse_interference_lookup();
}


void ForallPropagator::fixed(z3::expr const &action_variable, z3::expr const &value) {
    if (!value.is_true()) return; // Only process true assignments
    
    // Extract action and timestep from the variable
    auto action_info = variable_factory_->get_action_from_variable(action_variable);
    const Action& action = action_info->first;
    int timestep = action_info->second;
    
    // Do the propagation
    perform_forall_propagation(action, timestep, action_variable);
}

void ForallPropagator::perform_forall_propagation(const Action& action, int timestep, const z3::expr& action_var) {
    // Get node ID for the action
    int action_node_id = action.id();
    assert(action_node_id >= 0);
    
    // Get all node IDs that need to be negated when this action is true
    auto it = actions_interfering_with_.find(action_node_id);
    if (it == actions_interfering_with_.end()) { 
        return; // No actions interfere with this one
    }
    const std::set<int>& interfering_node_ids = it->second;

    // Collect all negations to propagate in one batch
    z3::expr_vector negations_to_propagate(ctx());
    for (int interfering_node_id : interfering_node_ids) {
        const Action* action_to_negate = &problem_->action(interfering_node_id);
        z3::expr interfering_var = variable_factory_->get_action_variable(*action_to_negate, timestep);
        negations_to_propagate.push_back(!interfering_var);
    }
    
    // Only propagate if we have negations to apply
    if (!negations_to_propagate.empty()) {
        // Increment propagation counter
        propagation_count_++;
        
        // Create justification for the propagated negations
        z3::expr_vector justification(ctx());
        justification.push_back(action_var);
        // create consequence
        z3::expr all_negations = mk_and(negations_to_propagate);
        // and propagate the big and of all negations
        propagate(justification, all_negations);
    }
}

void ForallPropagator::final() {
    // We are eager so we don't have to keep track of active actions
    // so this is effectively a NOOP
}

z3::user_propagator_base* ForallPropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    // TODO: Implement proper fresh instance creation if needed
    return nullptr;
}


void ForallPropagator::register_timestep_variables(int timestep) {
    const Z3VariableFactory& var_factory = *variable_factory_;
    // For timestep 0: register nothing as there are no actions
    if (timestep == 0) return;
    
    // For timestep t > 0: register action variables for t-1 

    // Check that we haven't already registered variables for timestep t-1?
    if (!registered_action_vars_.contains(timestep - 1)) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            // Register each action variable with Z3's user propagator framework
            // so that we get notified when any of these variables are assigned
            for (const auto& var_ptr : registered_action_vars_[timestep - 1]) {
                add(*var_ptr);
            }
        }
    }
}

void ForallPropagator::cleanup() {
    auto& stats = Stats::instance();
    stats.set("propagator.forall_total_propagations", propagation_count_);
}

PropagatorType ForallPropagator::get_type() const {
    return PropagatorType::FORALL;
}

void ForallPropagator::build_reverse_interference_lookup() {
    // Clear any existing data
    actions_interfering_with_.clear();
    
    // Build complete interference lookup: for each action, find all node IDs that need to be negated
    // This includes both incoming edges (actions that interfere with this action) 
    // and outgoing edges (actions that this action interferes with)
    for (const Action& action : problem_->actions()) {
        int node_id = action.id();
        
        // Get all actions that 'action' interferes with (outgoing edges)
        const std::vector<int>& interfered_with = 
            interference_analyzer_->get_interference_graph().get_neighbours(node_id);
        
        for (int target_node : interfered_with) {
            // Add outgoing edges: when 'action' is true, all actions it interferes with must be false
            actions_interfering_with_[node_id].insert(target_node);
            // As the graph has only directed edges, we have to build the reverse lookup: when the action
            // we are interfering with is true, the current action needs to be false.
            actions_interfering_with_[target_node].insert(node_id);
        }
    }
    
    std::cout << "Built reverse interference lookup for " 
              << actions_interfering_with_.size() << " node IDs" << std::endl;
}

} // namespace planmt