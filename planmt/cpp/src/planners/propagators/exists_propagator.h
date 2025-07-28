#pragma once

#include "propagator_strategy.h"
#include "../../problem/problem.h"
#include "../../problem/fluent.h"
#include "../../problem/action.h"
#include <z3++.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <vector>

namespace planmt {

/**
 * @brief Exists-specific user propagator implementation using incremental cycle detection
 * 
 * This class implements the exists propagation strategy where actions can execute
 * if there exists at least one order where they don't interfere. It uses an
 * incremental cycle detection algorithm to maintain a directed acyclic graph (DAG)
 * of action dependencies at each timestep, preventing cyclic dependencies.
 */
class ExistsPropagator : public z3::user_propagator_base, public PropagatorStrategy {
private:
    const GroundedEncoder* encoder_;  // Access to variable factory and problem
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

    // Per-timestep directed acyclic graphs for action ordering
    struct TimestepState {
        std::unordered_set<Action> current_actions;
        std::unordered_map<Action, std::unordered_set<Action>> ancestors;
        std::unordered_map<Action, std::unordered_set<Action>> descendants;
        std::vector<std::pair<Action, Action>> edge_trail;  // For undoing edge additions
        std::vector<std::tuple<Action, Action, Action>> ancestor_trail;  // For undoing ancestor additions
        std::vector<std::tuple<Action, Action, Action>> descendant_trail;  // For undoing descendant additions
        
        TimestepState() = default;
        
        // Custom copy constructor to handle the unordered containers
        TimestepState(const TimestepState& other) 
            : current_actions(other.current_actions),
              ancestors(other.ancestors),
              descendants(other.descendants),
              edge_trail(other.edge_trail),
              ancestor_trail(other.ancestor_trail),
              descendant_trail(other.descendant_trail) {}
        
        // Custom assignment operator
        TimestepState& operator=(const TimestepState& other) {
            if (this != &other) {
                current_actions = other.current_actions;
                ancestors = other.ancestors;
                descendants = other.descendants;
                edge_trail = other.edge_trail;
                ancestor_trail = other.ancestor_trail;
                descendant_trail = other.descendant_trail;
            }
            return *this;
        }
    };

    std::vector<TimestepState> timestep_states_;
    
    // Trail entries for state management
    std::vector<std::tuple<int, size_t, size_t, size_t>> trail_levels_;  // timestep, edge_trail_size, ancestor_trail_size, descendant_trail_size

    // Track registered variables by timestep
    std::unordered_map<int, std::vector<z3::expr>> registered_action_vars_;

    // Precomputed interference graph: action -> set of actions it interferes with
    std::unordered_map<Action, std::unordered_set<Action>> interference_graph_;

    bool consistent_;  // Flag to track consistency of the propagator
    
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
    
    // Incremental cycle detection algorithm
    bool incremental_cycle_detection(int timestep, const Action& source, const Action& dest);
    
    // Ensure timestep state exists
    void ensure_timestep_state(int timestep);
    
    // Build interference graph from parallelism strategy
    void build_interference_graph();
    
    // Debugging and utility methods
    void print_trail_state() const;
    void print_timestep_state(int timestep) const;
};

} // namespace planmt