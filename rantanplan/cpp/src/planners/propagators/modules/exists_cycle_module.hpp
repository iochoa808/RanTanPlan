#pragma once

#include "../propagator_module.hpp"
#include "../propagator_strategy.hpp"

namespace rantanplan {

/**
 * @brief Exists-step cycle detection module.
 *
 * When an action is fixed to true, performs DFS on the interference graph
 * among active actions at that timestep. If a cycle is found, reports a
 * conflict with all actions in the cycle.
 *
 * Reads from shared state: active_actions_per_timestep, potential_interferers,
 * interference analyzer, variable_factory, problem.
 * No private trail needed — cycle detection is stateless between callbacks.
 */
class ExistsCycleModule : public PropagatorModule {
public:
    std::string get_name() const override { return "ExistsCycle"; }
    bool manages_parallelism_constraints() const override { return true; }

    void on_fixed(const z3::expr& ast, const z3::expr& value) override;
    void cleanup() override;

private:
    int cycle_count_ = 0;

    void perform_exists_propagation(const Action& action, int timestep,
                                    const z3::expr& action_var);
    bool find_cycle_in_active_actions(
        const std::unordered_set<int>& active_node_ids,
        std::vector<int>& cycle);
};

} // namespace rantanplan
