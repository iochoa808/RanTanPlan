#pragma once

#include "propagator_strategy.hpp"
#include "../../problem/problem.hpp"
#include "../../problem/fluent.hpp"
#include "../../problem/action.hpp"
#include "../../analysis/graph.hpp"
#include <z3++.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rantanplan {

/**
 * @brief ExistsPropagator variant with footprint-indexed cycle detection.
 *
 * Identical to ExistsPropagator except the DFS in find_cycle_in_active_actions
 * only checks actions that share a read/write fluent (footprint neighbors)
 * instead of all active actions. This reduces per-DFS cost from O(|active|²)
 * to O(|footprint_neighbors ∩ active|).
 *
 * Strategies: exists-lazy-semantic-chain-foot, causal-exists-foot
 */
class FootprintExistsPropagator : public PropagatorStrategy {
private:
    const Problem* problem_;
    const Z3VariableFactory* variable_factory_;
    const ParallelismStrategy* parallelism_strategy_;
    const InterferenceAnalysis* interference_analyzer_;

    std::vector<std::pair<int, int>> trail_;
    std::vector<size_t> decision_levels_;
    std::unordered_map<int, std::unordered_set<int>> active_actions_per_timestep_;
    std::unordered_map<int, std::vector<std::shared_ptr<z3::expr>>> registered_action_vars_;
    int cycle_count_;

    // Footprint index: action_id → action_ids sharing a read/write fluent
    std::unordered_map<int, std::vector<int>> potential_interferers_;
    bool footprint_index_built_ = false;
    void build_footprint_index();

public:
    FootprintExistsPropagator(z3::solver& solver, const Problem& problem, const BaseEncoder& encoder);
    ~FootprintExistsPropagator() override = default;

    void on_push() override;
    void on_pop(unsigned num_scopes) override;
    void on_fixed(z3::expr const &ast, z3::expr const &value) override;

    void register_timestep_variables(int timestep) override;
    void cleanup() override;
    std::string get_name() const override { return "FootprintExistsPropagator"; }
    bool manages_parallelism_constraints() const override { return true; }

private:
    void perform_exists_propagation(const Action& action, int timestep, const z3::expr& action_var);
    bool find_cycle_in_active_actions(const std::unordered_set<int>& active_node_ids,
                                      std::vector<int>& cycle);
};

} // namespace rantanplan
