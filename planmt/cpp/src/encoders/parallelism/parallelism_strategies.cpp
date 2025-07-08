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
    // TODO: Implement forall parallelism semantics
    // This should encode constraints that prevent conflicting actions from executing simultaneously
    // For now, return a placeholder that allows any actions to execute
    
    std::cout << "Warning: ForallSemantics::encode_parallelism not yet implemented" << std::endl;
    std::cout << "         Falling back to allowing any number of actions (unlimited parallelism)" << std::endl;
    
    // Return true for now (no constraints = unlimited parallelism)
    return std::make_shared<z3::expr>(ctx_->bool_val(true));
}

std::shared_ptr<z3::expr> ForallSemantics::encode_mutex_constraints(int timestep) {
    // TODO: Implement mutex constraints using the interference graph
    // This would create constraints ensuring that no two interfering actions
    // can execute simultaneously at the given timestep
    
    const Graph& interference_graph = interference_analyzer_->get_interference_graph();
    
    std::cout << "ForallSemantics::encode_mutex_constraints called for timestep " << timestep << std::endl;
    std::cout << "Interference graph has " << interference_graph.num_nodes() << " nodes" << std::endl;
    
    // For now, return true (no mutex constraints implemented yet)
    return std::make_shared<z3::expr>(ctx_->bool_val(true));
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
    std::cout << "Warning: ExistsSemantics::encode_parallelism not yet implemented" << std::endl;
    std::cout << "         Falling back to allowing any number of actions (unlimited parallelism)" << std::endl;

    // Return true for now (no constraints = unlimited parallelism)
    return std::make_shared<z3::expr>(ctx_->bool_val(true));
}

} // namespace planmt
