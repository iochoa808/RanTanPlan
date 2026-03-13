#pragma once

#include "propagator_strategy.hpp"
#include "../../problem/problem.hpp"
#include "../../problem/fluent.hpp"
#include "../../problem/action.hpp"
#include "../../encoders/parallelism/graph.hpp"
#include <z3++.h>
#include <memory>
#include <set>
#include <unordered_map>

namespace rantanplan {

/**
 * @brief Forall propagator with interference checking. The idea
 * is simple: once an action is set to true by the solver, we check all its 
 * actions that can interfere with it and propagate a clause that should put 
 * all of them to false.
 * In other words, it propagates the set of mutexes in a compact way by
 * stating a_1 -> ¬a_2 ∧ ¬a_3 ∧ ...), which if expand, would be equivalent to
 * ¬a_1 ∨ (¬a_2 ∧ ¬a_3 ∧ ...) 
 * (¬a_1 ∨ ¬a_2) ∧ (¬a_1 ∨ ¬a_3) ∧ ...
 * Giving a big and of the mutexes :)
 */
class ForallPropagator : public PropagatorStrategy {
private:
    const Problem* problem_;    // Direct access to problem structure
    const Z3VariableFactory* variable_factory_;  // Cached reference to variable factory
    const ParallelismStrategy* parallelism_strategy_;  // Cached reference to parallelism strategy
    const InterferenceAnalysis* interference_analyzer_;  // Cached reference to interference analyzer

    // Track registered variables by timestep
    std::unordered_map<int, std::vector<std::shared_ptr<z3::expr>>> registered_action_vars_;

    // Precomputed complete interference lookup: node_id -> set of node_ids that need to be negated
    // to be able to do the check in constant time
    std::unordered_map<int, std::set<int>> actions_interfering_with_;

    // Counter for propagations made
    int propagation_count_;
    
public:
    /**
     * @brief Constructor that registers with a solver (required for callbacks)
     * @param solver Z3 solver to register with
     * @param problem Reference to the planning problem
     * @param encoder Reference to the encoder for variable factory access
     */
    ForallPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder);
    
    /**
     * @brief Destructor
     */
    ~ForallPropagator() override = default;
    
    // Propagator callbacks
    void on_fixed(z3::expr const &ast, z3::expr const &value) override;
    
    // PropagatorStrategy interface
    void register_timestep_variables(int timestep) override;
    void cleanup() override;
    std::string get_name() const override { return "ForallPropagator"; }
    bool manages_parallelism_constraints() const override { return true; }

private:
    // Simplified propagation logic
    void perform_forall_propagation(const Action& action, int timestep, const z3::expr& action_var);
    
    // Utility methods
    void build_reverse_interference_lookup();
};

} // namespace rantanplan