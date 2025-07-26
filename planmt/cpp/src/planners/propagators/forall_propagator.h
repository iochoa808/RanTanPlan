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
    std::unordered_map<int, std::unordered_set<Action>> active_actions_per_timestep_;
    
    // Track which action pairs have been propagated to avoid redundancy
    std::unordered_map<int, std::unordered_set<std::pair<Action, Action>>> propagated_pairs_;
    
    // Track registered variables by timestep
    std::unordered_map<int, std::vector<z3::expr>> registered_action_vars_;
    
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
    bool has_propagated_pair(const Action& a1, const Action& a2, int timestep) const;
    void mark_propagated_pair(const Action& a1, const Action& a2, int timestep);
    void print_trail_state() const;
};

} // namespace planmt

// Hash specialization for std::pair<Action, Action> for use in unordered containers
namespace std {
    template<>
    struct hash<std::pair<planmt::Action, planmt::Action>> {
        std::size_t operator()(const std::pair<planmt::Action, planmt::Action>& pair) const {
            // Combine hashes of both actions
            std::size_t h1 = std::hash<planmt::Action>()(pair.first);
            std::size_t h2 = std::hash<planmt::Action>()(pair.second);
            return h1 ^ (h2 << 1); // Simple hash combination
        }
    };
}
