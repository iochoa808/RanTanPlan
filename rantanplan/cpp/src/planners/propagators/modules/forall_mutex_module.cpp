#include "forall_mutex_module.hpp"
#include "../../../util/stats.hpp"

namespace rantanplan {

// ===========================================================================
// ForallMutexModule
// ===========================================================================

void ForallMutexModule::initialize(PropagatorSharedState& shared,
                                   PropagatorStrategy& host) {
    PropagatorModule::initialize(shared, host);
    build_reverse_interference_lookup();
}

void ForallMutexModule::on_fixed(const z3::expr& ast, const z3::expr& value) {
    if (!value.is_true()) return;
    auto action_info = shared_->variable_factory->get_action_from_variable(ast);
    if (!action_info) return;

    int action_node_id = action_info->first.id();
    int timestep = action_info->second;

    auto it = actions_interfering_with_.find(action_node_id);
    if (it == actions_interfering_with_.end()) return;

    z3::expr_vector negations(host_->z3_ctx());
    for (int interfering_id : it->second) {
        z3::expr var = shared_->variable_factory->get_action_variable(
            shared_->problem->action(interfering_id), timestep);
        negations.push_back(!var);
    }

    if (!negations.empty()) {
        propagation_count_++;
        z3::expr_vector justification(host_->z3_ctx());
        justification.push_back(ast);
        host_->module_propagate(justification, z3::mk_and(negations));
    }
}

void ForallMutexModule::cleanup() {
    Stats::instance().set("propagator.forall_total_propagations", propagation_count_);
}

void ForallMutexModule::build_reverse_interference_lookup() {
    actions_interfering_with_.clear();
    for (const Action& action : shared_->problem->actions()) {
        int node_id = action.id();
        const auto& neighbours =
            shared_->interference->get_interference_graph().get_neighbours(node_id);
        for (int target : neighbours) {
            actions_interfering_with_[node_id].insert(target);
            actions_interfering_with_[target].insert(node_id);
        }
    }
}

// ===========================================================================
// LazyForallMutexModule
// ===========================================================================

void LazyForallMutexModule::on_push() {
    // No private trail needed — shared action trail is managed by composite.
}

void LazyForallMutexModule::on_pop(unsigned /*num_scopes*/) {
    // Shared action trail is restored by composite.
}

void LazyForallMutexModule::on_fixed(const z3::expr& ast, const z3::expr& value) {
    if (!value.is_true()) return;
    auto action_info = shared_->variable_factory->get_action_from_variable(ast);
    if (!action_info) return;

    int current_id = action_info->first.id();
    int timestep = action_info->second;

    z3::expr_vector conflicting(host_->z3_ctx());
    for (int other_id : shared_->active_actions_per_timestep[timestep]) {
        if (other_id == current_id) continue;
        if (shared_->interference->has_interference(current_id, other_id) ||
            shared_->interference->has_interference(other_id, current_id)) {
            z3::expr var = shared_->variable_factory->get_action_variable(
                shared_->problem->action(other_id), timestep);
            conflicting.push_back(var);
        }
    }

    if (!conflicting.empty()) {
        conflict_count_++;
        z3::expr_vector justification(host_->z3_ctx());
        justification.push_back(ast);
        for (unsigned i = 0; i < conflicting.size(); ++i)
            justification.push_back(conflicting[i]);
        host_->module_conflict(justification);
    }
}

void LazyForallMutexModule::cleanup() {
    Stats::instance().set("propagator.lazy_forall_total_conflicts", conflict_count_);
}

} // namespace rantanplan
