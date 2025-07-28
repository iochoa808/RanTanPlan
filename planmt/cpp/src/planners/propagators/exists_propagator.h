#pragma once

#include "propagator_strategy.h"
#include "../../problem/problem.h"
#include "../../problem/fluent.h"
#include "../../problem/action.h"
#include <z3++.h>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <vector>
#include <set>

namespace planmt {

/**
 * @brief Exists-specific user propagator implementation using incremental cycle detection
 * 
 * This class implements the exists-step semantics. Actions can execute
 * if there exists at least one order where they don't interfere.
 *
 * - When action becomes true
 * - If that creates a cycle with the actions we have active
 * - Conflict with all actions participating in the cycle
 */
class ExistsPropagator : public z3::user_propagator_base, public PropagatorStrategy {
private:
    const GroundedEncoder* encoder_;  // Access to variable factory and problem
    const Problem* problem_;    // Direct access to problem structure
    const Z3VariableFactory* variable_factory_;  // Cached reference to variable factory

    // Trail-based state management for push/pop behavior
    struct TrailEntry {
        z3::expr variable;
        int timestep;
        Action action;
    };

    std::vector<TrailEntry> trail_;
    std::vector<size_t> decision_levels_;  // Indices into trail_ marking decision boundaries

    // Active actions tracking per timestep
    std::unordered_map<int, std::set<Action>> active_actions_per_timestep_;

    // Track registered variables by timestep
    std::unordered_map<int, std::vector<z3::expr>> registered_action_vars_;

    // Precomputed interference lookup: action -> set of interfering actions
    std::unordered_map<Action, std::set<Action>> interference_neighbours_;
    
public:
    /**
     * @brief Constructor that registers with a solver (required for callbacks)
     * @param solver Z3 solver to register with
     * @param problem Reference to the planning problem
     */
    ExistsPropagator(z3::solver& solver, const Problem& problem);
    
    /**
     * @brief Destructor
     */
    ~ExistsPropagator() override = default;
    
    // Z3 user_propagator_base interface
    void push() override;
    void pop(unsigned num_scopes) override;
    void fixed(z3::expr const &ast, z3::expr const &value) override;
    z3::user_propagator_base* fresh(z3::context& ctx) override;
    
    // PropagatorStrategy interface
    void initialize(z3::solver& solver, const GroundedEncoder& encoder) override;
    void register_timestep_variables(int timestep) override;
    void cleanup() override { } // Empty implementation for now
    std::string get_name() const override { return "ExistsPropagator"; }
    PropagatorType get_type() const override;

private:
    // Helper methods for exists propagation logic
    void perform_exists_propagation(const Action& action, int timestep, const z3::expr& action_var);
    
    // Cycle detection methods for active actions
    bool find_cycle_among_active_actions(const std::set<Action>& active_actions, 
                                         const std::unordered_map<Action, std::unordered_set<Action>>& successors,
                                         std::vector<Action>& cycle);
    bool find_cycle_dfs(const Action& current, const Action& target, 
                        const std::set<Action>& active_actions,
                        const std::unordered_map<Action, std::unordered_set<Action>>& successors,
                        std::unordered_set<Action>& visited, std::vector<Action>& path);
    
    void build_interference_lookup();

};

} // namespace planmt