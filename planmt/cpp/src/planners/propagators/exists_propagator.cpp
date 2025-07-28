#include "exists_propagator.h"
#include "../../encoders/z3_variable_factory.h"
#include "../../encoders/parallelism/interference_analyzer.h"
#include <iostream>
#include <set>
#include <algorithm>

namespace planmt {

ExistsPropagator::ExistsPropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(nullptr),
     variable_factory_(nullptr), consistent_(true) {
    // Define callbacks for the user propagator
    register_fixed();
}

void ExistsPropagator::push() {
    // Z3 is entering a new backtracking scope - mark decision level
    decision_levels_.push_back(trail_.size());
    
    // Record current trail sizes for each timestep
    for (size_t i = 0; i < timestep_states_.size(); ++i) {
        trail_levels_.push_back({
            static_cast<int>(i),
            timestep_states_[i].edge_trail.size(),
            timestep_states_[i].ancestor_trail.size(),
            timestep_states_[i].descendant_trail.size()
        });
    }
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
                
                // Remove action from current_actions set
                if (entry.timestep < static_cast<int>(timestep_states_.size())) {
                    timestep_states_[entry.timestep].current_actions.erase(entry.action);
                }
                
                trail_.pop_back();
            }
        }
        
        // Undo timestep state changes
        while (!trail_levels_.empty()) {
            auto [timestep, edge_size, ancestor_size, descendant_size] = trail_levels_.back();
            trail_levels_.pop_back();
            
            if (timestep < static_cast<int>(timestep_states_.size())) {
                TimestepState& state = timestep_states_[timestep];
                
                // Undo descendant trail entries
                while (state.descendant_trail.size() > descendant_size) {
                    auto [source, dest, descendant] = state.descendant_trail.back();
                    state.descendants[source].erase(descendant);
                    state.descendant_trail.pop_back();
                }
                
                // Undo ancestor trail entries
                while (state.ancestor_trail.size() > ancestor_size) {
                    auto [source, dest, ancestor] = state.ancestor_trail.back();
                    state.ancestors[dest].erase(ancestor);
                    state.ancestor_trail.pop_back();
                }
                
                // Undo edge trail entries (no actual edges to remove, just trail cleanup)
                while (state.edge_trail.size() > edge_size) {
                    state.edge_trail.pop_back();
                }
            }
        }
    }
    consistent_ = true; // Reset consistency flag on pop
}

void ExistsPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    if (!value.is_true() || !consistent_) {
        // Only process true assignments and if propagator is consistent
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
    trail_.push_back({ast, true, timestep, action});
    
    // Ensure timestep state exists
    ensure_timestep_state(timestep);
    
    // Add action to current actions set
    timestep_states_[timestep].current_actions.insert(action);
    
    // Initialize ancestors/descendants if not present
    if (timestep_states_[timestep].ancestors.find(action) == timestep_states_[timestep].ancestors.end()) {
        timestep_states_[timestep].ancestors[action] = std::unordered_set<Action>();
        timestep_states_[timestep].descendants[action] = std::unordered_set<Action>();
    }
    
    // Perform exists propagation logic
    perform_exists_propagation(action, timestep, ast);
}

void ExistsPropagator::perform_exists_propagation(const Action& action, int timestep, const z3::expr& action_var) {
    if (!consistent_) return;
    
    // Get all actions that interfere with this action
    auto it = interference_graph_.find(action);
    if (it == interference_graph_.end()) {
        return; // No interference relationships
    }
    
    const std::unordered_set<Action>& interfering_actions = it->second;
    const std::unordered_set<Action>& current_actions = timestep_states_[timestep].current_actions;
    
    // Check for incremental cycle detection with all interfering actions
    for (const Action& interfering_action : interfering_actions) {
        if (current_actions.find(interfering_action) != current_actions.end()) {
            // Both actions are active - check if we can add an ordering edge
            if (!incremental_cycle_detection(timestep, interfering_action, action)) {
                // Adding edge would create a cycle - conflict detected
                z3::expr interfering_var = variable_factory_->get_action_variable(interfering_action, timestep);
                z3::expr_vector conflict_vec(action_var.ctx());
                conflict_vec.push_back(action_var);
                conflict_vec.push_back(interfering_var);
                conflict(conflict_vec);
                consistent_ = false;
                return;
            }
        }
    }
    
    // Check reverse direction for all interfering actions
    for (const Action& interfering_action : interfering_actions) {
        if (current_actions.find(interfering_action) != current_actions.end()) {
            // Try the reverse edge as well
            if (!incremental_cycle_detection(timestep, action, interfering_action)) {
                // Adding reverse edge would also create a cycle - conflict detected
                z3::expr interfering_var = variable_factory_->get_action_variable(interfering_action, timestep);
                z3::expr_vector conflict_vec(action_var.ctx());
                conflict_vec.push_back(action_var);
                conflict_vec.push_back(interfering_var);
                conflict(conflict_vec);
                consistent_ = false;
                return;
            }
        }
    }
}

bool ExistsPropagator::incremental_cycle_detection(int timestep, const Action& source, const Action& dest) {
    TimestepState& state = timestep_states_[timestep];
    
    // Add edge to trail (for debugging/tracking)
    state.edge_trail.push_back({source, dest});
    
    // Check if adding this edge would create a cycle
    // A cycle exists if dest is already an ancestor of source, or if source == dest
    if (source == dest) {
        return false; // Self-loop would create cycle
    }
    
    const auto& source_ancestors = state.ancestors[source];
    if (source_ancestors.find(dest) != source_ancestors.end()) {
        return false; // dest is already an ancestor of source - would create cycle
    }
    
    // No immediate cycle - now propagate the ordering constraint
    // dest becomes a descendant of all ancestors of source
    const std::unordered_set<Action>& source_anc = state.ancestors[source];
    for (const Action& ancestor : source_anc) {
        if (state.descendants[ancestor].find(dest) == state.descendants[ancestor].end()) {
            state.descendants[ancestor].insert(dest);
            state.descendant_trail.push_back({source, dest, dest});
        }
        if (state.ancestors[dest].find(ancestor) == state.ancestors[dest].end()) {
            state.ancestors[dest].insert(ancestor);
            state.ancestor_trail.push_back({source, dest, ancestor});
        }
    }
    
    // source becomes an ancestor of dest
    if (state.ancestors[dest].find(source) == state.ancestors[dest].end()) {
        state.ancestors[dest].insert(source);
        state.ancestor_trail.push_back({source, dest, source});
    }
    if (state.descendants[source].find(dest) == state.descendants[source].end()) {
        state.descendants[source].insert(dest);
        state.descendant_trail.push_back({source, dest, dest});
    }
    
    // All descendants of dest become descendants of source and its ancestors
    const std::unordered_set<Action>& dest_desc = state.descendants[dest];
    for (const Action& descendant : dest_desc) {
        // source becomes ancestor of descendant
        if (state.ancestors[descendant].find(source) == state.ancestors[descendant].end()) {
            state.ancestors[descendant].insert(source);
            state.ancestor_trail.push_back({source, dest, source});
        }
        if (state.descendants[source].find(descendant) == state.descendants[source].end()) {
            state.descendants[source].insert(descendant);
            state.descendant_trail.push_back({source, dest, descendant});
        }
        
        // All ancestors of source become ancestors of descendant
        for (const Action& ancestor : source_anc) {
            if (state.ancestors[descendant].find(ancestor) == state.ancestors[descendant].end()) {
                state.ancestors[descendant].insert(ancestor);
                state.ancestor_trail.push_back({source, dest, ancestor});
            }
            if (state.descendants[ancestor].find(descendant) == state.descendants[ancestor].end()) {
                state.descendants[ancestor].insert(descendant);
                state.descendant_trail.push_back({source, dest, descendant});
            }
        }
    }
    
    return true; // No cycle detected
}

void ExistsPropagator::ensure_timestep_state(int timestep) {
    while (static_cast<int>(timestep_states_.size()) <= timestep) {
        timestep_states_.emplace_back();
    }
}

z3::user_propagator_base* ExistsPropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    // TODO: Implement proper fresh instance creation if needed
    return nullptr;
}

void ExistsPropagator::initialize(z3::solver& solver, const GroundedEncoder& encoder) {
    // Store reference to encoder for variable factory access
    encoder_ = &encoder;
    
    // Cache variable factory reference to avoid repeated lookups
    variable_factory_ = &encoder.get_variable_factory();
    
    // Set Z3 option to persist clauses for user propagator
    solver.set("smt.up.persist_clauses", true);
    
    // Build interference graph for efficient propagation
    build_interference_graph();
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

void ExistsPropagator::build_interference_graph() {
    const ParallelismStrategy* strategy = encoder_->get_parallelism_strategy();
    if (!strategy) return;
    
    const InterferenceAnalyzer* analyzer = strategy->get_interference_analyzer();
    if (!analyzer) return;
    
    // Clear any existing data
    interference_graph_.clear();
    
    // Build interference graph: for each action, find all actions it interferes with
    for (const Action& action : problem_->actions()) {
        Graph::NodeId node_id = analyzer->get_action_node_id(action);
        if (node_id < 0) continue;
        
        // Get all actions that 'action' interferes with
        const std::vector<Graph::NodeId>& interfered_with = 
            analyzer->get_interference_graph().get_neighbours(node_id);
        
        // Add to interference graph
        for (Graph::NodeId target_node : interfered_with) {
            const Action* target_action = analyzer->get_action_from_node_id(target_node);
            if (target_action) {
                interference_graph_[action].insert(*target_action);
                // Also add reverse direction for symmetric interference
                interference_graph_[*target_action].insert(action);
            }
        }
    }
    
    std::cout << "Built interference graph for " 
              << interference_graph_.size() << " actions" << std::endl;
}

void ExistsPropagator::print_trail_state() const {
    std::cout << "Trail: [";
    for (size_t i = 0; i < trail_.size(); ++i) {
        if (i > 0) std::cout << ", ";
        
        const TrailEntry& entry = trail_[i];
        std::cout << "'" << entry.action.name() << "_t" << entry.timestep 
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

void ExistsPropagator::print_timestep_state(int timestep) const {
    if (timestep >= static_cast<int>(timestep_states_.size())) {
        std::cout << "Timestep " << timestep << " not initialized" << std::endl;
        return;
    }
    
    const TimestepState& state = timestep_states_[timestep];
    std::cout << "Timestep " << timestep << ":" << std::endl;
    std::cout << "  Current actions: {";
    for (auto it = state.current_actions.begin(); it != state.current_actions.end(); ++it) {
        if (it != state.current_actions.begin()) std::cout << ", ";
        std::cout << it->name();
    }
    std::cout << "}" << std::endl;
    
    std::cout << "  Ancestors:" << std::endl;
    for (const auto& [action, ancestors] : state.ancestors) {
        std::cout << "    " << action.name() << ": {";
        for (auto it = ancestors.begin(); it != ancestors.end(); ++it) {
            if (it != ancestors.begin()) std::cout << ", ";
            std::cout << it->name();
        }
        std::cout << "}" << std::endl;
    }
}

} // namespace planmt