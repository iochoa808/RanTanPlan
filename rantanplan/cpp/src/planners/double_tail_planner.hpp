#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "../encoders/base_encoder.hpp"
#include "base_planner.hpp"
#include "propagators/propagator_strategy.hpp"
#include <z3++.h>
#include <memory>
#include <vector>

namespace rantanplan {

/**
 * @brief Double-tail (double-ended) incremental SAT planner
 *
 * This planner implements the double-tail incremental encoding from
 * "Accelerating SAT Based Planning with Incremental SAT Solving" (Gocht & Balyo, ICAPS 2017).
 *
 * Key idea:
 * - Build SAT formula from BOTH initial state (forward) AND goal state (backward)
 * - Alternates between adding forward and backward transitions
 * - Uses link constraints to connect the two stacks
 * - Iteration i tests for plans of exactly length i
 *
 * Algorithm:
 * - Iteration 0: I(0) ∧ G(n)
 * - Odd iterations (1, 3, 5, ...): Add forward transition, create link
 * - Even iterations (2, 4, 6, ...): Add backward transition, create link
 *
 * Benefits:
 * - Better learned clause reuse (goal constraints never deactivated)
 * - More constraints available earlier in search
 */
class DoubleTailPlanner : public BasePlanner {
public:
    /**
     * @brief Construct a double-tail planner
     * @param problem The planning problem
     * @param encoder The encoder to use (grounded, chained, etc.)
     * @param ctx Z3 context
     */
    DoubleTailPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx);

    // BasePlanner interface implementation
    Plan search() override;
    bool solution_found() const override { return solution_found_; }
    void set_propagator_strategy(std::unique_ptr<PropagatorStrategy> propagator) override;
    std::string get_propagator_strategy_name() const override;
    z3::solver& get_solver() override { return solver_; }

private:
    // Core data members
    const Problem& problem_;
    BaseEncoder& encoder_;
    z3::context& ctx_;
    z3::solver solver_;
    bool solution_found_ = false;
    std::unique_ptr<PropagatorStrategy> propagator_strategy_;
    int max_horizon_;  // Upper bound for backward stack timestep

    /**
     * @brief Create link constraints connecting forward and backward stacks
     * @param forward_t Timestep at end of forward stack
     * @param backward_t Timestep at start of backward stack
     * @return Conjunction of fluent equivalences: ∧fluents. fluent@forward_t ⟺ fluent@backward_t
     */
    std::shared_ptr<z3::expr> create_link_constraints(int forward_t, int backward_t);

    /**
     * @brief Add constraints for timestep t (actions + frames + parallelism)
     * @param t The timestep (encodes transition from t to t+1)
     *
     * Note: encode_actions(t) creates action variables at timestep t that cause
     * the transition from state t to state t+1. This is the same whether building
     * forward or backward - the encoding is purely based on the timestep number.
     */
    void add_timestep_constraints(int t);

    /**
     * @brief Extract plan from satisfying model
     * @param model The Z3 model containing satisfying assignment
     * @param plan_length Total plan length (iteration number)
     * @return Plan with forward actions followed by backward actions
     */
    Plan extract_plan(const z3::model& model, int plan_length);

    /**
     * @brief Extract all actions that are true at a given timestep
     * @param model The Z3 model
     * @param timestep The timestep to extract actions from
     * @return Vector of actions that execute at this timestep
     */
    std::vector<const Action*> extract_parallel_actions_at_timestep(
        const z3::model& model, int timestep) const;

    /**
     * @brief Extract and add actions from a range of timesteps to a plan
     * @param plan The plan to add actions to
     * @param model The Z3 model
     * @param start_timestep First timestep (inclusive)
     * @param end_timestep Last timestep (exclusive)
     */
    void extract_actions_in_range(Plan& plan, const z3::model& model,
                                   int start_timestep, int end_timestep);

    /**
     * @brief Topologically sort actions based on interference analysis
     * @param actions Actions to sort
     * @return Sorted actions (if sequential, returns at most one action)
     */
    std::vector<const Action*> topologically_sort_actions(
        const std::vector<const Action*>& actions) const;

    /**
     * @brief Calculate the forward stack depth at a given iteration
     * @param iteration Current iteration number
     * @return Number of forward actions (depth of forward stack)
     *
     * Forward stack grows on odd iterations, so forward_depth = ceil(iteration/2)
     * Using integer arithmetic: ceil(iteration/2) = (iteration + 1) / 2
     */
    int calculate_forward_depth(int iteration) const;

    /**
     * @brief Calculate the backward stack depth at a given iteration
     * @param iteration Current iteration number
     * @return Number of backward actions (depth of backward stack)
     *
     * Backward stack grows on even iterations, so backward_depth = floor(iteration/2)
     * Using integer arithmetic: floor(iteration/2) = iteration / 2
     */
    int calculate_backward_depth(int iteration) const;
};

} // namespace rantanplan
