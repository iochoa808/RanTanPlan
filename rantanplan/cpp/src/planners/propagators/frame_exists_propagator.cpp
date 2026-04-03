#include "frame_exists_propagator.hpp"
#include "../../config/config.hpp"
#include "../../util/stats.hpp"
#include "../../encoders/z3_variable_factory.hpp"
#include "../../encoders/grounded_encoder.hpp"
#include "../../analysis/interference_analysis.hpp"
#include "../../problem/visitors/fluent_collector.hpp"
#include <algorithm>
#include <functional>

namespace rantanplan {

FrameExistsPropagator::FrameExistsPropagator(z3::solver& solver, const Problem& problem,
                                             BaseEncoder& encoder)
    : PropagatorStrategy(solver, encoder),
      problem_(&problem),
      variable_factory_(&encoder.get_variable_factory()),
      parallelism_strategy_(encoder.get_parallelism_strategy()),
      interference_analyzer_(parallelism_strategy_->get_interference_analyzer()),
      solver_(&solver),
      encoder_nc_(&encoder),
      persist_clauses_(Config::instance().global.persist_clauses) {
    solver.set("smt.up.persist_clauses", persist_clauses_);
}

// ============================================================================
// Push / Pop
// ============================================================================

void FrameExistsPropagator::on_push() {
    decision_levels_.push_back(trail_.size());
    frame_decision_levels_.push_back(frame_trail_.size());
}

void FrameExistsPropagator::on_pop(unsigned num_scopes) {
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (!decision_levels_.empty()) {
            size_t level_start = decision_levels_.back();
            decision_levels_.pop_back();
            while (trail_.size() > level_start) {
                const auto& [action_node_id, timestep] = trail_.back();
                active_actions_per_timestep_[timestep].erase(action_node_id);
                trail_.pop_back();
            }
        }

        // Frame trail: restore only primary state, collect dirty clauses
        if (!frame_decision_levels_.empty()) {
            size_t frame_level_start = frame_decision_levels_.back();
            frame_decision_levels_.pop_back();

            std::vector<uint32_t> dirty_clauses;

            while (frame_trail_.size() > frame_level_start) {
                const auto& te = frame_trail_.back();
                auto& clause = frame_clauses_[te.clause_idx];

                switch (static_cast<VarRole::Kind>(te.kind)) {
                case VarRole::FLUENT_T:
                    clause.ft_val = te.prev_state;
                    break;
                case VarRole::FLUENT_T1:
                    clause.ft1_val = te.prev_state;
                    break;
                case VarRole::EQ_BOOL:
                    clause.eq_state = te.prev_state;
                    break;
                case VarRole::ACTION:
                    clause.entries[te.entry_idx].action_state = te.prev_state;
                    break;
                case VarRole::CONDITION:
                    clause.entries[te.entry_idx].cond_state = te.prev_state;
                    break;
                }
                dirty_clauses.push_back(te.clause_idx);
                frame_trail_.pop_back();
            }

            // Recompute derived state for dirty clauses
            std::sort(dirty_clauses.begin(), dirty_clauses.end());
            dirty_clauses.erase(
                std::unique(dirty_clauses.begin(), dirty_clauses.end()),
                dirty_clauses.end());
            for (uint32_t ci : dirty_clauses) {
                recompute_derived(frame_clauses_[ci]);
            }
        }
    }
}

// ============================================================================
// on_fixed
// ============================================================================

void FrameExistsPropagator::on_fixed(z3::expr const& ast, z3::expr const& value) {
    const bool val_true = value.is_true();

    // Exists cycle detection
    if (val_true) {
        auto action_info = variable_factory_->get_action_from_variable(ast);
        if (action_info) {
            const Action& action = action_info->first;
            int timestep = action_info->second;
            trail_.push_back({action.id(), timestep});
            active_actions_per_timestep_[timestep].insert(action.id());
            perform_exists_propagation(action, timestep, ast);
        }
    }

    // Frame axiom logic
    auto it = frame_var_to_roles_.find(ast.id());
    if (it == frame_var_to_roles_.end()) return;

    frame_on_fixed_count_++;
    const int8_t val_int = val_true ? 1 : 0;

    for (const VarRole& role : it->second) {
        auto& clause = frame_clauses_[role.clause_idx];

        // Compact trail: save only the primary field
        FrameTrailEntry te;
        te.clause_idx = static_cast<uint32_t>(role.clause_idx);
        te.kind = static_cast<uint8_t>(role.kind);
        te.entry_idx = static_cast<uint32_t>(role.entry_idx);

        switch (role.kind) {
        case VarRole::FLUENT_T: {
            te.prev_state = clause.ft_val;
            clause.ft_val = val_int;
            if (clause.is_boolean && clause.ft1_val >= 0)
                clause.eq_state = (clause.ft_val == clause.ft1_val) ? 1 : 0;
            break;
        }
        case VarRole::FLUENT_T1: {
            te.prev_state = clause.ft1_val;
            clause.ft1_val = val_int;
            if (clause.is_boolean && clause.ft_val >= 0)
                clause.eq_state = (clause.ft_val == clause.ft1_val) ? 1 : 0;
            break;
        }
        case VarRole::EQ_BOOL: {
            te.prev_state = clause.eq_state;
            clause.eq_state = val_int;
            break;
        }
        case VarRole::ACTION: {
            auto& entry = clause.entries[role.entry_idx];
            te.prev_state = entry.action_state;
            bool was_cant = entry.cant_explain();
            bool was_can = entry.can_explain();
            entry.action_state = val_int;
            bool now_cant = entry.cant_explain();
            bool now_can = entry.can_explain();
            if (now_cant != was_cant) clause.num_cant_explain += now_cant ? 1 : -1;
            if (now_can != was_can) clause.num_can_explain += now_can ? 1 : -1;
            clause.owned = (clause.num_can_explain > 0);
            break;
        }
        case VarRole::CONDITION: {
            auto& entry = clause.entries[role.entry_idx];
            te.prev_state = entry.cond_state;
            bool was_cant = entry.cant_explain();
            bool was_can = entry.can_explain();
            entry.cond_state = val_int;
            bool now_cant = entry.cant_explain();
            bool now_can = entry.can_explain();
            if (now_cant != was_cant) clause.num_cant_explain += now_cant ? 1 : -1;
            if (now_can != was_can) clause.num_can_explain += now_can ? 1 : -1;
            clause.owned = (clause.num_can_explain > 0);
            break;
        }
        }

        frame_trail_.push_back(te);

        check_frame_clause(clause, role.clause_idx);
    }
}

// ============================================================================
// on_final
// ============================================================================

void FrameExistsPropagator::on_final() {
    for (size_t ci = 0; ci < frame_clauses_.size(); ++ci) {
        auto& clause = frame_clauses_[ci];

        // eq_state == 1: fluent unchanged → frame trivially satisfied
        if (clause.eq_state == 1) continue;

        // owned: at least one modifier action is true → change is explained
        if (clause.owned) continue;

        // eq_state == 0 (changed) with an explaining action → OK
        if (clause.eq_state == 0 && clause.num_can_explain > 0) continue;

        // eq_state == 0, no explainer → definite violation
        if (clause.eq_state == 0) {
            frame_final_violation_count_++;
            report_frame_conflict(clause, ci);
            return;
        }

        // eq_state == -1 (unset): Z3 didn't decide whether the fluent changed.
        // No modifier action can explain a change, so the fluent MUST persist.
        // Propagate preservation to force f^t == f^{t+1}.
        if (clause.eq_state == -1 && clause.num_can_explain == 0) {
            frame_final_violation_count_++;
            propagate_fluent_preservation(clause, ci);
            return;
        }
    }
}

// ============================================================================
// Frame axiom core logic
// ============================================================================

void FrameExistsPropagator::check_frame_clause(FrameClause& clause, size_t clause_idx) {
    int n = static_cast<int>(clause.entries.size());

    if (clause.eq_state == 1) return;  // unchanged → satisfied
    if (clause.owned) return;          // owned by a true action → satisfied

    if (clause.num_cant_explain == n) {
        if (clause.eq_state == 0) {
            report_frame_conflict(clause, clause_idx);
        } else {
            propagate_fluent_preservation(clause, clause_idx);
        }
        return;
    }

    if (clause.eq_state == 0 && clause.num_can_explain > 0) return;

    if (clause.eq_state == 0 &&
        clause.num_cant_explain == n - 1 &&
        clause.num_can_explain == 0) {
        propagate_last_entry(clause, clause_idx);
    }
}

// ============================================================================
// Conflict & propagation
// ============================================================================

void FrameExistsPropagator::build_frame_fixed(
        z3::expr_vector& fixed,
        const FrameClause& clause, size_t clause_idx,
        bool include_change, int skip_idx) {
    if (include_change) {
        if (clause.is_boolean) {
            fixed.push_back(frame_fluent_ft_[clause_idx]);
            fixed.push_back(frame_fluent_ft1_[clause_idx]);
        } else {
            fixed.push_back(frame_eq_bool_[clause_idx]);
        }
    }

    const auto& actions = frame_action_expr_[clause_idx];
    const auto& conds = frame_cond_expr_[clause_idx];

    for (size_t i = 0; i < clause.entries.size(); ++i) {
        if (static_cast<int>(i) == skip_idx) continue;
        const auto& entry = clause.entries[i];

        if (entry.action_state == 0) {
            fixed.push_back(actions[i]);
        } else if (entry.is_conditional && entry.cond_state == 0) {
            fixed.push_back(actions[i]);
            fixed.push_back(conds[i]);
        }
    }
}

void FrameExistsPropagator::report_frame_conflict(const FrameClause& clause, size_t clause_idx) {
    z3::expr_vector fixed(ctx());
    build_frame_fixed(fixed, clause, clause_idx, true);
    frame_conflict_count_++;
    conflict(fixed);
}

void FrameExistsPropagator::propagate_fluent_preservation(const FrameClause& clause, size_t clause_idx) {
    z3::expr_vector fixed(ctx());
    build_frame_fixed(fixed, clause, clause_idx, false);

    frame_propagation_count_++;
    if (clause.is_boolean) {
        propagate(fixed, frame_fluent_ft1_[clause_idx] == frame_fluent_ft_[clause_idx]);
    } else {
        propagate(fixed, frame_eq_bool_[clause_idx]);
    }
}

void FrameExistsPropagator::propagate_last_entry(const FrameClause& clause, size_t clause_idx) {
    size_t idx = 0;
    for (size_t i = 0; i < clause.entries.size(); ++i) {
        if (!clause.entries[i].cant_explain() && !clause.entries[i].can_explain()) {
            idx = i;
            break;
        }
    }
    const auto& entry = clause.entries[idx];

    z3::expr_vector fixed(ctx());
    build_frame_fixed(fixed, clause, clause_idx, true, static_cast<int>(idx));

    if (entry.action_state == -1) {
        frame_propagation_count_++;
        propagate(fixed, frame_action_expr_[clause_idx][idx]);
    } else if (entry.is_conditional && entry.action_state == 1 && entry.cond_state == -1) {
        fixed.push_back(frame_action_expr_[clause_idx][idx]);
        frame_propagation_count_++;
        propagate(fixed, frame_cond_expr_[clause_idx][idx]);
    }
}

// ============================================================================
// Variable registration
// ============================================================================

void FrameExistsPropagator::register_timestep_variables(int timestep) {
    PropagatorStrategy::register_timestep_variables(timestep);

    const Z3VariableFactory& var_factory = *variable_factory_;

    if (timestep > 0 && !registered_action_vars_.contains(timestep - 1)) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            for (const auto& var_ptr : registered_action_vars_[timestep - 1]) {
                if (all_registered_ids_.insert(var_ptr->id()).second) {
                    add(*var_ptr);
                }
            }
        }
    }

    if (timestep > 0) {
        register_frame_variables(timestep - 1);
    }
}

void FrameExistsPropagator::register_frame_variables(int t) {
    auto* grounded = dynamic_cast<GroundedEncoder*>(encoder_nc_);
    if (!grounded) return;

    const auto& epc_index = grounded->get_epc_index();
    const Z3VariableFactory& var_factory = *variable_factory_;

    for (ExprID eid : problem_->grounded_fluents()) {
        auto epc_it = epc_index.find(eid);
        bool has_modifiers = (epc_it != epc_index.end() && !epc_it->second.empty());

        if (!has_modifiers) {
            // Fluent has no modifier actions → unconditional persistence.
            // No frame clause needed; just assert f^{t+1} = f^t directly.
            z3::expr f_t = grounded->convert_expr_id_to_z3(eid, t);
            z3::expr f_t1 = grounded->convert_expr_id_to_z3(eid, t + 1);
            solver_->add(f_t1 == f_t);
            continue;
        }

        const auto& action_effects = epc_it->second;

        bool is_bool = problem_->is_bool_type(eid);
        size_t clause_idx = frame_clauses_.size();

        z3::expr f_t = grounded->convert_expr_id_to_z3(eid, t);
        z3::expr f_t1 = grounded->convert_expr_id_to_z3(eid, t + 1);

        frame_clauses_.emplace_back(t, eid, is_bool);
        auto& clause = frame_clauses_.back();

        // Flat vectors for O(1) access during conflict/propagation
        frame_fluent_ft_.push_back(f_t);
        frame_fluent_ft1_.push_back(f_t1);

        if (is_bool) {
            frame_eq_bool_.push_back(ctx().bool_val(true)); // placeholder, unused for bool
            if (all_registered_ids_.insert(f_t.id()).second) add(f_t);
            frame_var_to_roles_[f_t.id()].push_back({VarRole::FLUENT_T, clause_idx, 0});
            if (all_registered_ids_.insert(f_t1.id()).second) add(f_t1);
            frame_var_to_roles_[f_t1.id()].push_back({VarRole::FLUENT_T1, clause_idx, 0});
        } else {
            std::string eq_name = "feq_" + std::to_string(clause_idx);
            z3::expr eq_bool = ctx().bool_const(eq_name.c_str());
            solver_->add(eq_bool == (f_t == f_t1));
            frame_eq_bool_.push_back(eq_bool);
            if (all_registered_ids_.insert(eq_bool.id()).second) add(eq_bool);
            frame_var_to_roles_[eq_bool.id()].push_back({VarRole::EQ_BOOL, clause_idx, 0});
        }

        // Per-entry action/condition expression vectors
        frame_action_expr_.emplace_back();
        frame_cond_expr_.emplace_back();
        auto& actions_vec = frame_action_expr_.back();
        auto& conds_vec = frame_cond_expr_.back();

        for (size_t i = 0; i < action_effects.size(); ++i) {
            const auto& [action, eff_expr] = action_effects[i];

            EPCEntry entry;
            entry.action = action;
            entry.is_conditional = eff_expr->is_conditional();

            z3::expr a = var_factory.get_action_variable(*action, t);
            actions_vec.push_back(a);

            if (all_registered_ids_.insert(a.id()).second) add(a);
            frame_var_to_roles_[a.id()].push_back({VarRole::ACTION, clause_idx, i});

            if (entry.is_conditional) {
                std::string cname = "fc_" + std::to_string(clause_idx) + "_" + std::to_string(i);
                z3::expr cond_reified = ctx().bool_const(cname.c_str());
                z3::expr cond_z3 = grounded->convert_expr_id_to_z3(eff_expr->condition_id(), t);
                solver_->add(cond_reified == cond_z3);
                conds_vec.push_back(cond_reified);
                if (all_registered_ids_.insert(cond_reified.id()).second) add(cond_reified);
                frame_var_to_roles_[cond_reified.id()].push_back({VarRole::CONDITION, clause_idx, i});
            } else {
                conds_vec.push_back(ctx().bool_val(true)); // placeholder
            }

            clause.entries.push_back(entry);
        }
    }
}

// ============================================================================
// Recompute derived counters from primary state
// ============================================================================

void FrameExistsPropagator::recompute_derived(FrameClause& clause) {
    if (clause.is_boolean) {
        if (clause.ft_val >= 0 && clause.ft1_val >= 0)
            clause.eq_state = (clause.ft_val == clause.ft1_val) ? 1 : 0;
        else
            clause.eq_state = -1;
    }
    clause.num_cant_explain = 0;
    clause.num_can_explain = 0;
    for (const auto& entry : clause.entries) {
        if (entry.cant_explain()) clause.num_cant_explain++;
        if (entry.can_explain()) clause.num_can_explain++;
    }
    clause.owned = (clause.num_can_explain > 0);
}

// ============================================================================
// Footprint-indexed interference neighbors
// ============================================================================

void FrameExistsPropagator::build_footprint_index() {
    if (footprint_index_built_) return;
    footprint_index_built_ = true;

    // For each fluent, collect which actions touch it (read or write).
    // Two actions that touch the same fluent are potential interferers.
    // fluent_eid → list of action IDs that touch it
    std::unordered_map<ExprID, std::vector<int>> fluent_to_actions;

    for (const Action& action : problem_->actions()) {
        int aid = action.id();

        // Write footprint: from effects
        for (const auto& effect : action.effects()) {
            ExprID fid = effect.effect_expression().fluent_id();
            fluent_to_actions[fid].push_back(aid);
        }

        // Read footprint: fluents appearing in preconditions
        if (action.has_precondition()) {
            FluentCollector collector(*problem_);
            collector.collect_from_id(action.precondition_id());
            for (ExprID fid : collector.get_fluents()) {
                fluent_to_actions[fid].push_back(aid);
            }
        }
    }

    // Build neighbor lists: for each fluent, all pairs of actions touching it
    // are potential interferers
    for (const auto& [fid, action_ids] : fluent_to_actions) {
        for (size_t i = 0; i < action_ids.size(); ++i) {
            for (size_t j = i + 1; j < action_ids.size(); ++j) {
                int a = action_ids[i], b = action_ids[j];
                if (a != b) {
                    potential_interferers_[a].push_back(b);
                    potential_interferers_[b].push_back(a);
                }
            }
        }
    }

    // Deduplicate neighbor lists
    for (auto& [aid, neighbors] : potential_interferers_) {
        std::sort(neighbors.begin(), neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
    }
}

// ============================================================================
// Exists-step cycle detection
// ============================================================================

void FrameExistsPropagator::perform_exists_propagation(
        const Action& action, int timestep, const z3::expr& action_var) {
    build_footprint_index();
    const std::unordered_set<int>& active_node_ids = active_actions_per_timestep_[timestep];
    std::vector<int> cycle;
    if (find_cycle_in_active_actions(active_node_ids, cycle)) {
        cycle_count_++;
        z3::expr_vector conflict_actions(action_var.ctx());
        for (int cycle_node_id : cycle) {
            const Action* cycle_action = &problem_->action(cycle_node_id);
            z3::expr cycle_var = variable_factory_->get_action_variable(*cycle_action, timestep);
            conflict_actions.push_back(cycle_var);
        }
        conflict(conflict_actions);
    }
}

bool FrameExistsPropagator::find_cycle_in_active_actions(
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

        // Use footprint index: only check actions sharing a read/write fluent.
        // Any truly interfering pair must share a fluent, so this is sound.
        auto fp_it = potential_interferers_.find(current);
        if (fp_it != potential_interferers_.end()) {
            for (int other_node : fp_it->second) {
                if (!active_node_ids.contains(other_node)) continue;
                if (interference_analyzer_->has_interference(current, other_node)) {
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

// ============================================================================
// Slack API
// ============================================================================
// Cleanup
// ============================================================================

void FrameExistsPropagator::cleanup() {
    auto& stats = Stats::instance();
    stats.set("propagator.exists_total_cycles", cycle_count_);
    stats.set("frame_prop.clauses_created", static_cast<double>(frame_clauses_.size()));
    stats.set("frame_prop.conflicts", static_cast<double>(frame_conflict_count_));
    stats.set("frame_prop.propagations", static_cast<double>(frame_propagation_count_));
    stats.set("frame_prop.on_fixed_calls", static_cast<double>(frame_on_fixed_count_));
    stats.set("frame_prop.on_final_violations", static_cast<double>(frame_final_violation_count_));
    stats.set("frame_prop.vars_registered", static_cast<double>(all_registered_ids_.size()));
}

} // namespace rantanplan
