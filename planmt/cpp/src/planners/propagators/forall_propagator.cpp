#include "forall_propagator.h"
#include "../../config/config.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analysis.h"
#include <iostream>
#include <set>

namespace planmt {

ForallPropagator::ForallPropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(nullptr),
     variable_factory_(nullptr) {
    // Define callbacks for the user propagator
    register_fixed();
}

void ForallPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
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
    
    // Perform forall propagation logic
    perform_forall_propagation(action, timestep, ast);  
}

void ForallPropagator::perform_forall_propagation(const Action& action, int timestep, const z3::expr& action_var) {
    const ParallelismStrategy* strategy = encoder_->get_parallelism_strategy();
    if (!strategy) return;
    
    const InterferenceAnalysis* analyzer = strategy->get_interference_analyzer();
    if (!analyzer) return;
    
    // Get node ID for the action
    Graph::NodeId action_node_id = analyzer->get_action_node_id(action);
    if (action_node_id < 0) return;
    
    // Get all node IDs that need to be negated when this action is true
    auto it = actions_interfering_with_.find(action_node_id);
    if (it == actions_interfering_with_.end()) { 
        return; // No actions interfere with this one
    }
    const std::set<Graph::NodeId>& interfering_node_ids = it->second;
    
    // Collect all negations to propagate in one batch
    z3::expr_vector negations_to_propagate(action_var.ctx());
    for (Graph::NodeId interfering_node_id : interfering_node_ids) {
        const Action* action_to_negate = analyzer->get_action_from_node_id(interfering_node_id);
        if (action_to_negate) {
            z3::expr interfering_var = variable_factory_->get_action_variable(*action_to_negate, timestep);
            z3::expr negated_var = !interfering_var;
            negations_to_propagate.push_back(negated_var);
        }
    }
    
    // Only propagate if we have negations to apply
    if (negations_to_propagate.size() > 0) {
        // Create justification for the propagated negations
        z3::expr_vector justification(action_var.ctx());
        justification.push_back(action_var);
        // create consequence
        z3::expr all_negations = mk_and(negations_to_propagate);
        // and propagate the big and of all negations
        propagate(justification, all_negations);
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
    
    // Set Z3 option to persist clauses for user propagator based on config
    solver.set("smt.up.persist_clauses", Config::instance().propagators.persist_clauses);
    
    // Build reverse interference lookup for efficient propagation
    build_reverse_interference_lookup();
}

void ForallPropagator::register_timestep_variables(int timestep) {
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

PropagatorType ForallPropagator::get_type() const {
    return PropagatorType::FORALL;
}

void ForallPropagator::build_reverse_interference_lookup() {
    const ParallelismStrategy* strategy = encoder_->get_parallelism_strategy();
    if (!strategy) return;
    
    const InterferenceAnalysis* analyzer = strategy->get_interference_analyzer();
    if (!analyzer) return;
    
    // Clear any existing data
    actions_interfering_with_.clear();
    
    // Build complete interference lookup: for each action, find all node IDs that need to be negated
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
            actions_interfering_with_[node_id].insert(target_node);
        }
        
        // Add incoming edges: when 'action' is true, all actions that interfere with it must be false
        // (This builds the reverse lookup from the outgoing edges of other actions)
        for (Graph::NodeId target_node : interfered_with) {
            actions_interfering_with_[target_node].insert(node_id);
        }
    }
    
    std::cout << "Built reverse interference lookup for " 
              << actions_interfering_with_.size() << " node IDs" << std::endl;
}

} // namespace planmt