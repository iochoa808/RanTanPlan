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
 * @brief Forall-specific user propagator implementation
 * 
 * This class implements both the Z3 user_propagator_base interface and the
 * PropagatorStrategy interface, providing custom propagation logic for
 * planning problems. It can distinguish between state and action variables
 * and has access to the variable factory for reverse lookups.
 */
class ForallPropagator : public z3::user_propagator_base, public PropagatorStrategy {
private:
    const GroundedEncoder* encoder_;  // Access to variable factory and problem (const for initialization)
    const Problem* problem_;    // Direct access to problem structure
    const Z3VariableFactory* variable_factory_;  // Cached reference to variable factory

    // Trail-based state management for push/pop behavior
    struct TrailEntry {
        z3::expr variable;
        bool value;
        int timestep;
        Action action;
    };

    std::vector<TrailEntry> trail_;
    std::vector<size_t> decision_levels_;  // Indices into trail_ marking decision boundaries

    // Active actions tracking per timestep
    std::unordered_map<int, std::set<Action>> active_actions_per_timestep_;

    // Track registered variables by timestep
    std::unordered_map<int, std::vector<z3::expr>> registered_action_vars_;

    // Precomputed complete interference lookup: action -> set of actions that need to be negated
    std::unordered_map<Action, std::set<Action>> actions_interfering_with_;

    bool consistent_;  // Flag to track consistency of the propagator
    
public:
    /**
     * @brief Constructor that registers with a solver (required for callbacks)
     * @param solver Z3 solver to register with
     * @param problem Reference to the planning problem
     * 
     * Note: ForallPropagator requires solver access to receive callbacks.
     * Use the factory to create instances properly.
     */
    ForallPropagator(z3::solver& solver, const Problem& problem);
    
    /**
     * @brief Destructor
     */
    ~ForallPropagator() override = default;
    
    // Z3 user_propagator_base interface
    void push() override;
    void pop(unsigned num_scopes) override;
    void fixed(z3::expr const &ast, z3::expr const &value) override;
    z3::user_propagator_base* fresh(z3::context& ctx) override;
    
    // PropagatorStrategy interface
    void initialize(z3::solver& solver, const GroundedEncoder& encoder) override;
    void register_timestep_variables(int timestep) override;
    void cleanup() override { } // Empty implementation for now
    std::string get_name() const override { return "ForallPropagator"; }
    PropagatorType get_type() const override;

private:
    // Helper methods for forall propagation logic
    void perform_forall_propagation(const Action& action, int timestep, const z3::expr& action_var);
    
    // Debugging and utility methods
    void print_trail_state() const;
    void build_reverse_interference_lookup();
};

} // namespace planmt