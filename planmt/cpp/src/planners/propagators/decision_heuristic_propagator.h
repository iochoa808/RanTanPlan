#pragma once

#include "propagator_strategy.h"
#include "../../problem/problem.h"
#include "../../problem/fluent.h"
#include "../../problem/action.h"
#include "../../encoders/parallelism/graph.h"
#include <z3++.h>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <vector>
#include <set>

namespace planmt {

/**
 * @brief Exists-specific user propagator implementation using incremental cycle detection
 *        It also includes some decision heuristics to improve propagation efficiency.
 *
 * - When action becomes true
 * - If that creates a cycle with the actions we have active
 * - Conflict with all actions participating in the cycle
 *
 * - When the solver needs to decide on the next action:
 * - For now does nothing
 */
class DecisionHeuristicPropagator : public z3::user_propagator_base, public PropagatorStrategy {
private:
    const BaseEncoder* encoder_;  // Access to variable factory and problem
    const Problem* problem_;    // Direct access to problem structure
    const Z3VariableFactory* variable_factory_;  // Cached reference to variable factory
    const ParallelismStrategy* parallelism_strategy_;  // Cached reference to parallelism strategy
    const InterferenceAnalysis* interference_analyzer_;  // Cached reference to interference analyzer

    // Trail-based state management for push/pop behavior (using NodeIds for efficiency)
    std::vector<std::pair<Graph::NodeId, int>> trail_;  // (action_node_id, timestep)
    std::vector<size_t> decision_levels_;  // Indices into trail_ marking decision boundaries

    // Active actions tracking per timestep (using NodeIds for efficiency)
    std::unordered_map<int, std::unordered_set<Graph::NodeId>> active_actions_per_timestep_;

    // Track registered variables by timestep
    std::unordered_map<int, std::vector<z3::expr>> registered_action_vars_;

    // Counter for detected cycles
    int cycle_count_;


    
public:
    /**
     * @brief Constructor that registers with a solver (required for callbacks)
     * @param solver Z3 solver to register with
     * @param problem Reference to the planning problem
     * @param encoder Reference to the encoder for variable factory access
     */
    DecisionHeuristicPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder);
    
    /**
     * @brief Destructor
     */
    ~DecisionHeuristicPropagator() override = default;
    
    // Z3 user_propagator_base interface
    void push() override;
    void pop(unsigned num_scopes) override;
    void fixed(z3::expr const &ast, z3::expr const &value) override;
    void decide(z3::expr const& val, unsigned bit, bool is_pos) override;
    void final() override;
    z3::user_propagator_base* fresh(z3::context& ctx) override;
    
    // PropagatorStrategy interface
    void register_timestep_variables(int timestep) override;
    void cleanup() override;
    std::string get_name() const override { return "DecisionHeuristicPropagator"; }
    PropagatorType get_type() const override;
    

private:
    // Helper methods for exists propagation logic
    void perform_exists_propagation(const Action& action, int timestep, const z3::expr& action_var);
    
    // Cycle detection method for active actions using node IDs
    bool find_cycle_in_active_actions(const std::unordered_set<Graph::NodeId>& active_node_ids, 
                                      std::vector<Graph::NodeId>& cycle);
    
    // Decision helper methods
    std::vector<z3::expr> get_decision_candidates() const;
    z3::expr select_next_split(const std::vector<z3::expr>& candidates) const;

};

} // namespace planmt