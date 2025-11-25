#include "forall_semantics.hpp"
#include "../../problem/action.hpp"
#include "../../util/stats.hpp"
#include <iostream>

namespace rantanplan {

void ForallSemantics::initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) {
    problem_ = &problem;
    ctx_ = &ctx;
    variable_factory_ = &var_factory;

    std::cout << "ForallSemantics initialized" << std::endl;
}

std::shared_ptr<z3::expr> ForallSemantics::encode_parallelism(int timestep) {
    // Get the interference graph from the analyzer
    const Graph& interference_graph = interference_analyzer_->get_interference_graph();
    
    z3::expr_vector mutex_constraints(*ctx_);
    
    // Iterate through all nodes (actions) in the interference graph
    for (size_t node_id = 0; node_id < interference_graph.num_nodes(); ++node_id) {
        // Get the action corresponding to this node
        const Action* action1 = &problem_->action(node_id);
        if (!action1) continue;
        
        // Get all neighbors (actions that this action interferes with)
        const std::vector<int>& neighbors = interference_graph.get_neighbours(node_id);
        
        for (int neighbor_id : neighbors) {
            // Get the second action
            const Action* action2 = &problem_->action(neighbor_id);
            if (!action2) continue;
            
            // Create Z3 variables for both actions at the given timestep
            z3::expr action1_var = variable_factory_->get_action_variable(*action1, timestep);
            z3::expr action2_var = variable_factory_->get_action_variable(*action2, timestep);
            
            // Add mutex constraint: not both actions can be true at the same time
            z3::expr mutex_constraint = !(action1_var && action2_var);
            mutex_constraints.push_back(mutex_constraint);
        }
    }
    
    // Combine all mutex constraints with logical AND
    if (mutex_constraints.empty()) {
        std::cout << "ForallSemantics: Generated 0 mutex constraints for timestep " << timestep << std::endl;
        return std::make_shared<z3::expr>(ctx_->bool_val(true));
    }
    
    std::cout << "ForallSemantics: Generated " << mutex_constraints.size() << " mutex constraints for timestep " << timestep << std::endl;
    
    auto& stats = Stats::instance();
    stats.add("encoder.mutex_constraints_per_step", mutex_constraints.size());
    
    z3::expr mutex_formula = z3::mk_and(mutex_constraints);
    return std::make_shared<z3::expr>(mutex_formula);
}

const InterferenceAnalysis* ForallSemantics::get_interference_analyzer() const {
    return interference_analyzer_.get();
}

} // namespace rantanplan
