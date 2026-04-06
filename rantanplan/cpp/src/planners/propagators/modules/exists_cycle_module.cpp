#include "exists_cycle_module.hpp"
#include "../../../util/stats.hpp"
#include <algorithm>
#include <functional>

namespace rantanplan {

void ExistsCycleModule::on_fixed(const z3::expr& ast, const z3::expr& value) {
    if (!value.is_true()) return;
    auto action_info = shared_->variable_factory->get_action_from_variable(ast);
    if (!action_info) return;

    perform_exists_propagation(action_info->first, action_info->second, ast);
}

void ExistsCycleModule::perform_exists_propagation(const Action& action,
                                                   int timestep,
                                                   const z3::expr& action_var) {
    shared_->build_footprint_index();
    const auto& active = shared_->active_actions_per_timestep[timestep];

    std::vector<int> cycle;
    if (find_cycle_in_active_actions(active, cycle)) {
        cycle_count_++;

        z3::expr_vector conflict_actions(host_->z3_ctx());
        for (int nid : cycle) {
            z3::expr var = shared_->variable_factory->get_action_variable(
                shared_->problem->action(nid), timestep);
            conflict_actions.push_back(var);
        }
        host_->module_conflict(conflict_actions);
    }
}

bool ExistsCycleModule::find_cycle_in_active_actions(
        const std::unordered_set<int>& active_node_ids,
        std::vector<int>& cycle) {
    if (active_node_ids.size() < 2) return false;

    std::unordered_set<int> visited;
    std::unordered_set<int> recursion_stack;
    std::vector<int> path;

    std::function<bool(int)> dfs = [&](int current) -> bool {
        visited.insert(current);
        recursion_stack.insert(current);
        path.push_back(current);

        auto fp_it = shared_->potential_interferers.find(current);
        if (fp_it != shared_->potential_interferers.end()) {
            for (int other_node : fp_it->second) {
                if (!active_node_ids.contains(other_node)) continue;
                if (shared_->interference->has_interference(current, other_node)) {
                    if (recursion_stack.contains(other_node)) {
                        auto cycle_start = std::find(path.begin(), path.end(), other_node);
                        cycle.assign(cycle_start, path.end());
                        return true;
                    }
                    if (!visited.contains(other_node)) {
                        if (dfs(other_node)) return true;
                    }
                }
            }
        }

        recursion_stack.erase(current);
        path.pop_back();
        return false;
    };

    for (int start_node : active_node_ids) {
        if (!visited.count(start_node)) {
            if (dfs(start_node)) return true;
        }
    }
    return false;
}

void ExistsCycleModule::cleanup() {
    Stats::instance().set("propagator.exists_total_cycles", cycle_count_);
}

} // namespace rantanplan
