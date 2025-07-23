#include "sample_propagator.h"
#include "../../encoders/z3_variable_factory.h"
#include <iostream>

namespace planmt {

SamplePropagator::SamplePropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(nullptr), fixed_values_(solver.ctx()) {
    
    // Define callbacks for the user propagator
    register_fixed();
    register_final();
    
    // Initialize storage for fixed variables per scope
    fixed_cnt_.push(0);
}

void SamplePropagator::push() {
    // Z3 is entering a new backtracking scope
    fixed_cnt_.push(fixed_values_.size());
}

void SamplePropagator::pop(unsigned num_scopes) {
    // Z3 is backtracking
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (!fixed_cnt_.empty()) {
            unsigned last_cnt = fixed_cnt_.top();
            fixed_cnt_.pop();
            
            // Remove fixed values that were added after this scope
            fixed_values_.resize(last_cnt);
        }
    }
}

void SamplePropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    // This is called whenever Z3 fixes a variable to a value
    fixed_values_.push_back(ast);
    //std::cout << "SamplePropagator::fixed() called for variable: " 
    //          << ast.to_string() << " with value: " << value.to_string() << std::endl;  
    if (!encoder_) {
        // Not initialized yet, ignore
        return;
    }
    
    const Z3VariableFactory& var_factory = encoder_->get_variable_factory();
    
    if (var_factory.is_fluent_variable(ast)) {
        auto fluent_info = var_factory.get_fluent_from_variable(ast);
        if (fluent_info) {
            handle_state_variable_fixed(ast, value, fluent_info->first, fluent_info->second);
        }
    } else if (var_factory.is_action_variable(ast)) {
        auto action_info = var_factory.get_action_from_variable(ast);
        if (action_info) {
            handle_action_variable_fixed(ast, value, action_info->first, action_info->second);
        }
    }
}

void SamplePropagator::final() {
    // This is called when Z3 finds a satisfying assignment
    // Can be used to add additional constraints or conflicts
    //std::cout << "SamplePropagator::final() called - model found" << std::endl;
}

z3::user_propagator_base* SamplePropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    // TODO: Implement proper fresh instance creation if needed
    return nullptr;
}

void SamplePropagator::initialize(z3::solver& solver, const GroundedEncoder& encoder) {
    // Store reference to encoder for variable factory access
    encoder_ = &encoder;
}

void SamplePropagator::register_timestep_variables(int timestep) {
    if (!encoder_) {
        throw std::runtime_error("SamplePropagator not initialized before registering variables");
    }
    
    const Z3VariableFactory& var_factory = encoder_->get_variable_factory();
    //std::cout << "Registering variables for timestep " << timestep << std::endl;
    
    // For timestep 0: register initial state fluents + goal_at_0
    if (timestep == 0) {
        // Get fluent variables for initial state (timestep 0)
        auto fluent_vars = var_factory.get_all_fluent_variables(0);
        if (!fluent_vars.empty()) {
            registered_state_vars_[0] = std::move(fluent_vars);
            //std::cout << "Registered " << registered_state_vars_[0].size() 
            //          << " initial state fluent variables for timestep 0" << std::endl;
            
            // Register these with Z3 user propagator
            for (const auto& var : registered_state_vars_[0]) {
                add(var);
                //std::cout << "  Registered with Z3: " << var.to_string() << std::endl;
            }
        }
        //std::cout << "Variable registration completed for timestep 0 (initial state)" << std::endl;
        return;
    }
    
    // For timestep t > 0: register action variables for t-1, fluent variables for t, and goal for t
    
    // 1. Register action variables for timestep t-1 (just encoded by encode_actions(t-1))
    if (registered_action_vars_.find(timestep - 1) == registered_action_vars_.end()) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            //std::cout << "Registered " << registered_action_vars_[timestep - 1].size() 
            //          << " action variables for timestep " << (timestep - 1) << std::endl;
            
            // Register these with Z3 user propagator
            for (const auto& var : registered_action_vars_[timestep - 1]) {
                add(var);
                //std::cout << "  Registered with Z3: " << var.to_string() << std::endl;
            }
        }
    }
    
    // 2. Register fluent variables for timestep t (from encode_frames(t-1) which creates timestep t)
    if (var_factory.has_variables_for_timestep(timestep)) {
        auto fluent_vars = var_factory.get_all_fluent_variables(timestep);
        if (!fluent_vars.empty()) {
            registered_state_vars_[timestep] = std::move(fluent_vars);
            //std::cout << "Registered " << registered_state_vars_[timestep].size() 
            //          << " fluent variables for timestep " << timestep << std::endl;
            
            // Register these with Z3 user propagator
            for (const auto& var : registered_state_vars_[timestep]) {
                add(var);
                //std::cout << "  Registered with Z3: " << var.to_string() << std::endl;
            }
        }
    }
    //std::cout << "Variable registration completed for timestep " << timestep << std::endl;
}

void SamplePropagator::handle_state_variable_fixed(
    const z3::expr& var, 
    const z3::expr& value,
    const Fluent& fluent, 
    int timestep) {
    
    //std::cout << "State variable fixed: " << fluent.name() 
    //          << " at timestep " << timestep 
    //          << " = " << value << std::endl;
    
    // TODO: Add domain-specific propagation logic here
    // For example:
    // - Propagate mutex constraints
    // - Check goal conditions
    // - Propagate causal dependencies
}

void SamplePropagator::handle_action_variable_fixed(
    const z3::expr& var, 
    const z3::expr& value,
    const Action& action, 
    int timestep) {
    
    std::cout << "Action variable fixed: " << action.name() 
              << " at timestep " << timestep 
              << " = " << value << std::endl;
    
    // TODO: Add domain-specific propagation logic here
    // For example:
    // - Propagate action preconditions and effects
    // - Handle action exclusivity constraints
    // - Trigger resource constraints
}

} // namespace planmt
