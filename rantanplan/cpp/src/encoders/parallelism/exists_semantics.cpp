#include "exists_semantics.hpp"
#include "../../problem/action.hpp"
#include "../../util/stats.hpp"

namespace rantanplan {

std::shared_ptr<z3::expr> ExistsSemantics::encode_parallelism(int timestep) {
    // Get the interference graph from the analyzer
    const Graph& interference_graph = interference_analyzer_->get_interference_graph();
    
    // Get SCC mapping for optimization: only add mutex within same SCC
    std::vector<int> scc_mapping = interference_graph.get_scc_mapping();
    
    z3::expr_vector mutex_constraints(*ctx_);
    
    // Iterate through all nodes (actions) in the interference graph
    for (size_t node_id = 0; node_id < interference_graph.num_nodes(); ++node_id) {
        // Get the action corresponding to this node
        const Action* action1 = &problem_->action(node_id);
        if (!action1) continue;
        
        // Get all neighbors (actions that this action interferes with)
        const std::vector<int>& neighbors = interference_graph.get_neighbours(node_id);
        
        for (int neighbor_id : neighbors) {
            // Enhanced conditions for ExistsSemantics:
            // 1. Only create mutex if source node ID > target node ID
            // 2. Only create mutex if both nodes are in the same SCC
            if (static_cast<int>(node_id) > neighbor_id && 
                scc_mapping[node_id] == scc_mapping[neighbor_id]) {
                
                // Get the second action
                const Action* action2 = &problem_->action(neighbor_id);
                if (!action2) continue;
                
                // Create Z3 variables for both actions at the given timestep
                z3::expr action1_var = variable_factory_->get_action_variable(*action1, timestep);
                z3::expr action2_var = variable_factory_->get_action_variable(*action2, timestep);
                
                // Create mutex constraint: ¬(action1_t ∧ action2_t) ≡ (¬action1_t ∨ ¬action2_t)
                z3::expr mutex = !action1_var || !action2_var;
                mutex_constraints.push_back(mutex);
            }
        }
    }
    
    // Combine all mutex constraints with logical AND
    if (mutex_constraints.empty()) {
        return std::make_shared<z3::expr>(ctx_->bool_val(true));
    }

    auto& stats = Stats::instance();
    stats.add("encoder.mutex_constraints", mutex_constraints.size());
    
    z3::expr mutex_formula = z3::mk_and(mutex_constraints);
    return std::make_shared<z3::expr>(mutex_formula);
}

const InterferenceAnalysis* ExistsSemantics::get_interference_analyzer() const {
    return interference_analyzer_.get();
}

} // namespace rantanplan
