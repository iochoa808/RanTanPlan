#include "parallelism_strategies.h"
#include "../../problem/action.h"
#include <iostream>


namespace planmt {

// ====================== SequentialSemantics ======================

void SequentialSemantics::initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) {
    problem_ = &problem;
    ctx_ = &ctx;
    variable_factory_ = &var_factory;
}

std::shared_ptr<z3::expr> SequentialSemantics::encode_parallelism(int timestep) {
    // Create a vector of all action variables at timestep
    z3::expr_vector action_vars(*ctx_);
    for (const Action& action : problem_->actions()) {
        z3::expr action_var = variable_factory_->get_action_variable(action, timestep);
        action_vars.push_back(action_var);
    }
    
    if (action_vars.empty()) {
        // If no actions exist, return true (vacuously satisfied)
        return std::make_shared<z3::expr>(ctx_->bool_val(true));
    }
    
    // Create pseudo-boolean constraint: exactly 1 action is true
    // Use Z3's dedicated pseudo-boolean equality constraint with all coefficients = 1
    std::vector<int> coeffs(action_vars.size(), 1);
    z3::expr exactly_one = z3::pbeq(action_vars, coeffs.data(), 1);
    return std::make_shared<z3::expr>(exactly_one);
}

// ====================== ForallSemantics ======================

void ForallSemantics::initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) {
    problem_ = &problem;
    ctx_ = &ctx;
    variable_factory_ = &var_factory;
    
    // Create and initialize the interference analyzer
    interference_analyzer_ = std::make_unique<InterferenceAnalyzer>(problem);
    
    std::cout << "ForallSemantics initialized with interference analysis" << std::endl;
}

std::shared_ptr<z3::expr> ForallSemantics::encode_parallelism(int timestep) {
    // Get the interference graph from the analyzer
    const Graph& interference_graph = interference_analyzer_->get_interference_graph();
    
    z3::expr_vector mutex_constraints(*ctx_);
    
    // Iterate through all nodes (actions) in the interference graph
    for (size_t node_id = 0; node_id < interference_graph.num_nodes(); ++node_id) {
        // Get the action corresponding to this node
        const Action* action1 = interference_analyzer_->get_action_from_node_id(node_id);
        if (!action1) continue;
        
        // Get all neighbors (actions that this action interferes with)
        const std::vector<Graph::NodeId>& neighbors = interference_graph.get_neighbours(node_id);
        
        for (Graph::NodeId neighbor_id : neighbors) {
            // Get the second action
            const Action* action2 = interference_analyzer_->get_action_from_node_id(neighbor_id);
            if (!action2) continue;
            
            // Create Z3 variables for both actions at the given timestep
            z3::expr action1_var = variable_factory_->get_action_variable(*action1, timestep);
            z3::expr action2_var = variable_factory_->get_action_variable(*action2, timestep);
            
            // Create mutex constraint: ¬(action1_t ∧ action2_t) ≡ (¬action1_t ∨ ¬action2_t)
            z3::expr mutex = !action1_var || !action2_var;
            mutex_constraints.push_back(mutex);
        }
    }
    
    // Combine all mutex constraints with logical AND
    if (mutex_constraints.empty()) {
        std::cout << "ForallSemantics: Generated 0 mutex constraints for timestep " << timestep << std::endl;
        return std::make_shared<z3::expr>(ctx_->bool_val(true));
    }
    
    std::cout << "ForallSemantics: Generated " << mutex_constraints.size() << " mutex constraints for timestep " << timestep << std::endl;
    z3::expr mutex_formula = z3::mk_and(mutex_constraints);
    return std::make_shared<z3::expr>(mutex_formula);
}

const InterferenceAnalyzer* ForallSemantics::get_interference_analyzer() const {
    return interference_analyzer_.get();
}

// ====================== ExistsSemantics ======================

void ExistsSemantics::initialize(const Problem& problem, z3::context& ctx, Z3VariableFactory& var_factory) {
    problem_ = &problem;
    ctx_ = &ctx;
    variable_factory_ = &var_factory;
    
    // Create and initialize the interference analyzer
    interference_analyzer_ = std::make_unique<InterferenceAnalyzer>(problem);
    
    std::cout << "ExistsSemantics initialized with interference analysis" << std::endl;
}

std::shared_ptr<z3::expr> ExistsSemantics::encode_parallelism(int timestep) {
    // Get the interference graph from the analyzer
    const Graph& interference_graph = interference_analyzer_->get_interference_graph();
    
    // Get SCC mapping for optimization: only add mutex within same SCC
    std::vector<int> scc_mapping = interference_graph.get_scc_mapping();
    
    z3::expr_vector mutex_constraints(*ctx_);
    
    // Iterate through all nodes (actions) in the interference graph
    for (size_t node_id = 0; node_id < interference_graph.num_nodes(); ++node_id) {
        // Get the action corresponding to this node
        const Action* action1 = interference_analyzer_->get_action_from_node_id(node_id);
        if (!action1) continue;
        
        // Get all neighbors (actions that this action interferes with)
        const std::vector<Graph::NodeId>& neighbors = interference_graph.get_neighbours(node_id);
        
        for (Graph::NodeId neighbor_id : neighbors) {
            // Enhanced conditions for ExistsSemantics:
            // 1. Only create mutex if source node ID > target node ID
            // 2. Only create mutex if both nodes are in the same SCC
            if (static_cast<int>(node_id) > neighbor_id && 
                scc_mapping[node_id] == scc_mapping[neighbor_id]) {
                
                // Get the second action
                const Action* action2 = interference_analyzer_->get_action_from_node_id(neighbor_id);
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
        std::cout << "ExistsSemantics: Generated 0 mutex constraints for timestep " << timestep << std::endl;
        return std::make_shared<z3::expr>(ctx_->bool_val(true));
    }
    
    std::cout << "ExistsSemantics: Generated " << mutex_constraints.size() << " mutex constraints for timestep " << timestep << std::endl;
    z3::expr mutex_formula = z3::mk_and(mutex_constraints);
    return std::make_shared<z3::expr>(mutex_formula);
}

const InterferenceAnalyzer* ExistsSemantics::get_interference_analyzer() const {
    return interference_analyzer_.get();
}

} // namespace planmt
