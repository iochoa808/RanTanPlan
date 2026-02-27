#pragma once

#include "propagator_strategy.hpp"
#include "../../problem/problem.hpp"
#include "../../problem/fluent.hpp"
#include "../../problem/action.hpp"
#include "../../encoders/parallelism/graph.hpp"
#include "../../abstraction/achievers_analysis.hpp"
#include <z3++.h>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <vector>
#include <set>
#include <functional>
#include <variant>

namespace rantanplan {

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
class DecisionHeuristicPropagator : public PropagatorStrategy {
private:
    z3::solver* solver_;  // Reference to the main solver for adding constraints
    const Problem* problem_;    // Direct access to problem structure
    const Z3VariableFactory* variable_factory_;  // Cached reference to variable factory
    const ParallelismStrategy* parallelism_strategy_;  // Cached reference to parallelism strategy
    const InterferenceAnalysis* interference_analyzer_;  // Cached reference to interference analyzer
    
    // Achievers analysis for condition tracking
    AchieversAnalysis achievers_analysis_;

    // Trail-based state management for push/pop behavior
    // Store z3::expr directly - determine type (action/reification) during pop using existing logic
    std::vector<z3::expr> trail_;
    std::vector<size_t> decision_levels_;  // Indices into trail_ marking decision boundaries

    // Active actions tracking per timestep (using ints for efficiency)
    std::unordered_map<int, std::unordered_set<int>> active_actions_per_timestep_;
    
    // Inactive actions tracking per timestep (actions assigned to false)
    std::unordered_map<int, std::unordered_set<int>> inactive_actions_per_timestep_;

    // Track registered variables by timestep
    std::unordered_map<int, std::vector<std::shared_ptr<z3::expr>>> registered_action_vars_;

    // Counter for detected cycles
    int cycle_count_;

    // Track reification variables for achievers analysis conditions
    // Vector of maps indexed by timestep: reification_vars_per_timestep_[timestep][condition_eid] = reif_var
    std::vector<std::unordered_map<ExprID, std::shared_ptr<z3::expr>>> reification_vars_per_timestep_;

    // O(1) lookup maps for reification variables (similar to Z3VariableFactory pattern)
    // Maps from variable name to (condition ExprID, timestep) for fast reverse lookup
    std::unordered_map<std::string, std::pair<ExprID, int>> reification_var_name_to_condition_;

    // Counter for generating unique reification variable IDs
    int reification_counter_;

    // Efficient condition value tracking per timestep using existing trail system
    // condition_values_per_timestep_[timestep][condition_eid] = current_value
    std::vector<std::unordered_map<ExprID, Z3_lbool>> condition_values_per_timestep_;

    // Stack for tracking subgoals
    int current_goal_step_;
    mutable std::vector<std::pair<ExprID, int>> subgoal_stack_;

    
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
    
    // Propagator callbacks
    void on_push() override;
    void on_pop(unsigned num_scopes) override;
    void on_fixed(z3::expr const &ast, z3::expr const &value) override;
    void on_decide(z3::expr const& val, unsigned bit, bool is_pos) override;
    
    // PropagatorStrategy interface
    void register_timestep_variables(int timestep) override;
    void cleanup() override;
    std::string get_name() const override { return "DecisionHeuristicPropagator"; }
    bool manages_parallelism_constraints() const override { return true; }
    

private:
    // Helper methods for exists propagation logic
    void perform_exists_propagation(const Action& action, int timestep, const z3::expr& action_var);
    
    // Cycle detection method for active actions using node IDs
    bool find_cycle_in_active_actions(const std::unordered_set<int>& active_node_ids, 
                                      std::vector<int>& cycle);
    
    // Support finding methods (Figure 2 from paper)
    std::optional<std::pair<Action, int>> find_support() const;
    
    // Condition reification methods
    void create_condition_reification_variables(int timestep);
    void reification_variable_assigned(const z3::expr& ast, const z3::expr& value);
    
    // O(1) lookup methods for variable type checking
    bool is_reification_variable(const z3::expr& var) const;
    bool is_action_variable(const z3::expr& var) const;
    std::optional<std::pair<ExprID, int>> get_condition_from_reification_variable(const z3::expr& var) const;

    // Condition value query methods
    bool has_condition_value(ExprID condition, int timestep) const;
    Z3_lbool get_condition_value(ExprID condition, int timestep) const;
    
    // Debug/utility methods
    void print_condition_values() const;
    void print_action_condition_status(const Action& action, int timestep) const;
};

} // namespace rantanplan