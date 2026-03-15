#include "sequential_semantics.hpp"
#include "../../problem/action.hpp"
#include "../../util/stats.hpp"

namespace rantanplan {

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
    
    // Create pseudo-boolean constraint: AT MOST 1 action is true (pble = pseudo-boolean <=).
    // Changed from pbeq (exactly-one) to pble (at-most-one) so that empty steps are legal.
    // Empty steps are required for the prefix-monotone front-loading encoding: when a plan
    // of length k < h exists, the solver uses empty steps k..h-1 so frame axioms propagate
    // the goal state to horizon h, allowing a single goal literal per scheduled batch.
    // With a unit schedule this is behaviourally equivalent to the old exactly-one.
    std::vector<int> coeffs(action_vars.size(), 1);
    z3::expr exactly_one = z3::pble(action_vars, coeffs.data(), 1);
    
    auto& stats = Stats::instance();
    stats.add("encoder.parallelism_sequential_constraints", 1);
    
    return std::make_shared<z3::expr>(exactly_one);
}

} // namespace rantanplan
