#pragma once

#include "propagator_strategy.h"
#include "../../problem/problem.h"
#include "../../problem/fluent.h"
#include "../../problem/action.h"
#include <z3++.h>
#include <memory>
#include <unordered_map>
#include <stack>

namespace planmt {

/**
 * @brief Sample user propagator implementation
 * 
 * This class is a simple template code that implements both the Z3
 * user_propagator_base interface and the PropagatorStrategy interface,
 * providing the scaffolding for custom propagation logic. It can distinguish
 * between state and action variables and has access to the variable factory for
 * reverse lookups.
 */
class SamplePropagator : public z3::user_propagator_base, public PropagatorStrategy {
private:
    const GroundedEncoder* encoder_;  // Access to variable factory and problem (const for initialization)
    const Problem* problem_;    // Direct access to problem structure
    
    // Internal state tracking for propagation
    z3::expr_vector fixed_values_;
    std::stack<unsigned> fixed_cnt_;
    
    // Track registered variables by timestep
    std::unordered_map<int, std::vector<z3::expr>> registered_state_vars_;
    std::unordered_map<int, std::vector<z3::expr>> registered_action_vars_;
    
public:
    /**
     * @brief Constructor that registers with a solver (required for callbacks)
     * @param solver Z3 solver to register with
     * @param problem Reference to the planning problem
     * 
     * Note: SamplePropagator requires solver access to receive callbacks.
     * Use the factory to create instances properly.
     */
    SamplePropagator(z3::solver& solver, const Problem& problem);
    
    /**
     * @brief Destructor
     */
    ~SamplePropagator() override = default;
    
    // Z3 user_propagator_base interface
    void push() override;
    void pop(unsigned num_scopes) override;
    void fixed(z3::expr const &ast, z3::expr const &value) override;
    void final() override;
    z3::user_propagator_base* fresh(z3::context& ctx) override;
    
    // PropagatorStrategy interface
    void initialize(z3::solver& solver, const GroundedEncoder& encoder) override;
    void register_timestep_variables(int timestep) override;
    void cleanup() override { } // Empty implementation for now
    std::string get_name() const override { return "SamplePropagator"; }

private:
    /**
     * @brief Handle a state variable being fixed to a value
     * @param var The Z3 variable that was fixed
     * @param value The value it was fixed to
     * @param fluent The fluent this variable represents
     * @param timestep The timestep of this variable
     */
    void handle_state_variable_fixed(const z3::expr& var, const z3::expr& value, 
                                   const Fluent& fluent, int timestep);
    
    /**
     * @brief Handle an action variable being fixed to a value
     * @param var The Z3 variable that was fixed
     * @param value The value it was fixed to
     * @param action The action this variable represents
     * @param timestep The timestep of this variable
     */
    void handle_action_variable_fixed(const z3::expr& var, const z3::expr& value, 
                                    const Action& action, int timestep);
};

} // namespace planmt
