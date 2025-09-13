#pragma once

#include "propagator_strategy.h"
#include "../../problem/problem.h"
#include "../../problem/fluent.h"
#include "../../problem/action.h"
#include "../../encoders/parallelism/graph.h"
#include <z3++.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace planmt {

/**
 * @brief LazyForall propagator with on-demand interference checking. This propagator
 * dynamically checks for interference between active actions at each timestep.
 * When an action is set to true, it checks all other active actions at the same
 * timestep for interference and generates conflicts when interference is detected.
 * This approach avoids precomputing the full interference graph.
 */
class LazyForallPropagator : public z3::user_propagator_base, public PropagatorStrategy {
private:
    const BaseEncoder* encoder_;  // Access to variable factory and problem
    const Problem* problem_;    // Direct access to problem structure
    const Z3VariableFactory* variable_factory_;  // Cached reference to variable factory
    const ParallelismStrategy* parallelism_strategy_;  // Cached reference to parallelism strategy
    const InterferenceAnalysis* interference_analyzer_;  // Cached reference to interference analyzer

    // Track registered variables by timestep
    std::unordered_map<int, std::vector<std::shared_ptr<z3::expr>>> registered_action_vars_;

    // Trail-based state management for push/pop behavior (using ints for efficiency)
    std::vector<std::pair<int, int>> trail_;  // (action_node_id, timestep)
    std::vector<size_t> decision_levels_;  // Indices into trail_ marking decision boundaries

    // Active actions tracking per timestep (using ints for efficiency)
    std::unordered_map<int, std::unordered_set<int>> active_actions_per_timestep_;

    // Counter for conflicts thrown
    int conflict_count_;
    
public:
    /**
     * @brief Constructor that registers with a solver (required for callbacks)
     * @param solver Z3 solver to register with
     * @param problem Reference to the planning problem
     * @param encoder Reference to the encoder for variable factory access
     */
    LazyForallPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder);
    
    /**
     * @brief Destructor
     */
    ~LazyForallPropagator() override = default;
    
    // Z3 user_propagator_base interface
    void push() override;
    void pop(unsigned num_scopes) override;
    void fixed(z3::expr const &ast, z3::expr const &value) override;
    void final() override;
    z3::user_propagator_base* fresh(z3::context& ctx) override;
    
    // PropagatorStrategy interface
    void register_timestep_variables(int timestep) override;
    void cleanup() override;
    std::string get_name() const override { return "LazyForallPropagator"; }
    PropagatorType get_type() const override;

private:
    // Dynamic interference checking and conflict generation
    void check_and_generate_conflicts(const Action& action, int timestep, const z3::expr& action_var);
};

} // namespace planmt