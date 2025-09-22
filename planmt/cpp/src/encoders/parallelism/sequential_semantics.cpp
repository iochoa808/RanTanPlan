#include "sequential_semantics.hpp"
#include "../../problem/action.hpp"
#include "../../util/stats.hpp"
#include <iostream>

namespace planmt {

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
    
    auto& stats = Stats::instance();
    stats.add("encoder.parallelism_sequential_constraints", 1);
    
    return std::make_shared<z3::expr>(exactly_one);
}

} // namespace planmt
