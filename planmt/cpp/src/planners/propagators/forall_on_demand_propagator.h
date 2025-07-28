#pragma once

#include "propagator_strategy.h"
#include "../../problem/problem.h"
#include "../../problem/fluent.h"
#include "../../problem/action.h"
#include <z3++.h>
#include <memory>
#include <unordered_map>

namespace planmt {

/**
 * @brief Simplified forall propagator with on-demand interference checking
 * 
 * This class implements both the Z3 user_propagator_base interface and the
 * PropagatorStrategy interface, providing simplified propagation logic that
 * negates all interfering actions without conflict detection or trail management.
 */
class ForallOnDemandPropagator : public z3::user_propagator_base, public PropagatorStrategy {
private:
    const GroundedEncoder* encoder_;  // Access to variable factory and problem
    const Problem* problem_;    // Direct access to problem structure
    const Z3VariableFactory* variable_factory_;  // Cached reference to variable factory

    // Track registered variables by timestep
    std::unordered_map<int, std::vector<z3::expr>> registered_action_vars_;

    // Precomputed complete interference lookup: action -> set of actions that need to be negated
    std::unordered_map<Action, std::set<Action>> actions_interfering_with_;
    
public:
    /**
     * @brief Constructor that registers with a solver (required for callbacks)
     * @param solver Z3 solver to register with
     * @param problem Reference to the planning problem
     */
    ForallOnDemandPropagator(z3::solver& solver, const Problem& problem);
    
    /**
     * @brief Destructor
     */
    ~ForallOnDemandPropagator() override = default;
    
    // Z3 user_propagator_base interface (simplified)
    void push() override { /* No-op - no state to track */ }
    void pop(unsigned num_scopes) override { /* No-op - no state to undo */ }
    void fixed(z3::expr const &ast, z3::expr const &value) override;
    z3::user_propagator_base* fresh(z3::context& ctx) override;
    
    // PropagatorStrategy interface
    void initialize(z3::solver& solver, const GroundedEncoder& encoder) override;
    void register_timestep_variables(int timestep) override;
    void cleanup() override { } // Empty implementation
    std::string get_name() const override { return "ForallOnDemandPropagator"; }
    PropagatorType get_type() const override;

private:
    // Simplified propagation logic
    void perform_forall_propagation(const Action& action, int timestep, const z3::expr& action_var);
    
    // Utility methods
    void build_reverse_interference_lookup();
};

} // namespace planmt