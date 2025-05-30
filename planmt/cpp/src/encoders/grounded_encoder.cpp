#include "grounded_encoder.h"
#include <iostream>

namespace planmt {

// Constructor
GroundedEncoder::GroundedEncoder(const Problem& problem, z3::context& ctx)
    : problem_(problem), ctx_(ctx), symbol_cache_() {
    // Initialize storage for state and action variables
    state_vars_.clear();
    action_vars_.clear();
    layers_encoded_ = -1;
}

// Encoding steps
std::shared_ptr<z3::expr> GroundedEncoder::encode_initial_state() {
    // TODO: Implement initial state encoding
    for (const auto& assignment : problem_.initial_state()) {
        const Expression& fluent = assignment.fluent();
        const Expression& value = assignment.value();
        
        // print the expression and the type of both operands
        std::cout << "Fluent = " << fluent.to_string() 
                  << ", Value = " << value.to_string() 
                  << ", Kinds = (" << fluent.kind() << "," << value.kind() << ")" << std::endl;

        // use the encoding visitor to encode the fluent
        // z3::expr fluent_var = get_fluent_var(fluent, assignment.parameters(), 0);


        // Get the fluent variable for the initial state
        //z3::expr fluent_var = get_fluent_var(fluent, assignment.parameters(), 0);
        //z3::expr initial_constraint = fluent_var == ctx_.bool_val(assignment.value());
        //*expr = *expr && initial_constraint;
    }

    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_actions(int t) {
    // TODO: Implement action encoding for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_frames(int t) {
    // TODO: Implement frame axioms encoding for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_goal_state(int t) {
    // TODO: Implement goal state encoding for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

std::shared_ptr<z3::expr> GroundedEncoder::encode_parallelism(int t) {
    // TODO: Implement parallelism constraints for timestep t
    auto expr = std::make_shared<z3::expr>(ctx_.bool_val(true));
    return expr;
}

// Private helper methods
z3::expr GroundedEncoder::get_fluent_var(const Fluent& fluent, const std::vector<Object>& params, int t) {
    std::string var_name = get_smt_var_name(fluent, params, t);
    
    // Ensure we have enough timesteps allocated
    while (static_cast<int>(state_vars_.size()) <= t) {
        state_vars_.emplace_back();
    }
    
    auto& timestep_vars = state_vars_[t];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        // Create new variable
        auto new_var = std::make_shared<z3::expr>(ctx_.bool_const(var_name.c_str()));
        timestep_vars[var_name] = new_var;
        return *new_var;
    }
    return *(it->second);
}

z3::expr GroundedEncoder::get_action_var(const Action& action, const std::vector<Object>& params, int t) {
    std::string var_name = get_smt_var_name(action, params, t);
    
    // Ensure we have enough timesteps allocated
    while (static_cast<int>(action_vars_.size()) <= t) {
        action_vars_.emplace_back();
    }
    
    auto& timestep_vars = action_vars_[t];
    auto it = timestep_vars.find(var_name);
    if (it == timestep_vars.end()) {
        // Create new variable
        auto new_var = std::make_shared<z3::expr>(ctx_.bool_const(var_name.c_str()));
        timestep_vars[var_name] = new_var;
        return *new_var;
    }
    return *(it->second);
}

std::string GroundedEncoder::get_smt_var_name(const Fluent& fluent, const std::vector<Object>& params) const {
    std::string name = fluent.name();
    for (const auto& param : params) {
        name += "_" + param.name();
    }
    return name;
}

std::string GroundedEncoder::get_smt_var_name(const Fluent& fluent, const std::vector<Object>& params, int t) const {
    return get_smt_var_name(fluent, params) + "_t" + std::to_string(t);
}

std::string GroundedEncoder::get_smt_var_name(const Action& action, const std::vector<Object>& params) const {
    std::string name = action.name();
    for (const auto& param : params) {
        name += "_" + param.name();
    }
    return name;
}

std::string GroundedEncoder::get_smt_var_name(const Action& action, const std::vector<Object>& params, int t) const {
    return get_smt_var_name(action, params) + "_t" + std::to_string(t);
}

} // namespace planmt