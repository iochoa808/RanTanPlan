#include "frame_axiom_module.hpp"
#include "../../../config/config.hpp"
#include "../../../util/stats.hpp"
#include "../../../encoders/grounded_encoder.hpp"
#include <algorithm>

namespace rantanplan {

// ============================================================================
// Push / Pop (private frame trail only)
// ============================================================================

void FrameAxiomModule::on_push() {
    frame_decision_levels_.push_back(frame_trail_.size());
}

void FrameAxiomModule::on_pop(unsigned num_scopes) {
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (frame_decision_levels_.empty()) continue;
        size_t frame_level_start = frame_decision_levels_.back();
        frame_decision_levels_.pop_back();

        std::vector<uint32_t> dirty_clauses;

        while (frame_trail_.size() > frame_level_start) {
            const auto& te = frame_trail_.back();
            auto& clause = frame_clauses_[te.clause_idx];

            switch (static_cast<VarRole::Kind>(te.kind)) {
            case VarRole::FLUENT_T:
                clause.ft_val = te.prev_state; break;
            case VarRole::FLUENT_T1:
                clause.ft1_val = te.prev_state; break;
            case VarRole::EQ_BOOL:
                clause.eq_state = te.prev_state; break;
            case VarRole::ACTION:
                clause.entries[te.entry_idx].action_state = te.prev_state; break;
            case VarRole::CONDITION:
                clause.entries[te.entry_idx].cond_state = te.prev_state; break;
            }
            dirty_clauses.push_back(te.clause_idx);
            frame_trail_.pop_back();
        }

        std::sort(dirty_clauses.begin(), dirty_clauses.end());
        dirty_clauses.erase(
            std::unique(dirty_clauses.begin(), dirty_clauses.end()),
            dirty_clauses.end());
        for (uint32_t ci : dirty_clauses)
            recompute_derived(frame_clauses_[ci]);
    }
}

// ============================================================================
// on_fixed
// ============================================================================

void FrameAxiomModule::on_fixed(const z3::expr& ast, const z3::expr& value) {
    auto it = frame_var_to_roles_.find(ast.id());
    if (it == frame_var_to_roles_.end()) return;

    frame_on_fixed_count_++;
    const int8_t val_int = value.is_true() ? 1 : 0;

    for (const VarRole& role : it->second) {
        auto& clause = frame_clauses_[role.clause_idx];

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

        if (host_->conflict_raised()) return;
        check_frame_clause(clause, role.clause_idx);
        if (host_->conflict_raised()) return;
    }
}

// ============================================================================
// on_final
// ============================================================================

void FrameAxiomModule::on_final() {
    for (size_t ci = 0; ci < frame_clauses_.size(); ++ci) {
        auto& clause = frame_clauses_[ci];

        if (clause.eq_state == 1) continue;
        if (clause.owned) continue;
        if (clause.eq_state == 0 && clause.num_can_explain > 0) continue;

        if (clause.eq_state == 0) {
            frame_final_violation_count_++;
            report_frame_conflict(clause, ci);
            return;
        }

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

void FrameAxiomModule::check_frame_clause(FrameClause& clause, size_t clause_idx) {
    int n = static_cast<int>(clause.entries.size());

    if (clause.eq_state == 1) return;
    if (clause.owned) return;

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

void FrameAxiomModule::build_frame_fixed(
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

void FrameAxiomModule::report_frame_conflict(const FrameClause& clause, size_t clause_idx) {
    z3::expr_vector fixed(host_->z3_ctx());
    build_frame_fixed(fixed, clause, clause_idx, true);
    frame_conflict_count_++;
    host_->module_conflict(fixed);
}

void FrameAxiomModule::propagate_fluent_preservation(const FrameClause& clause, size_t clause_idx) {
    z3::expr_vector fixed(host_->z3_ctx());
    build_frame_fixed(fixed, clause, clause_idx, false);

    frame_propagation_count_++;
    if (clause.is_boolean) {
        host_->module_propagate(fixed, frame_fluent_ft1_[clause_idx] == frame_fluent_ft_[clause_idx]);
    } else {
        host_->module_propagate(fixed, frame_eq_bool_[clause_idx]);
    }
}

void FrameAxiomModule::propagate_last_entry(const FrameClause& clause, size_t clause_idx) {
    size_t idx = 0;
    for (size_t i = 0; i < clause.entries.size(); ++i) {
        if (!clause.entries[i].cant_explain() && !clause.entries[i].can_explain()) {
            idx = i;
            break;
        }
    }
    const auto& entry = clause.entries[idx];

    z3::expr_vector fixed(host_->z3_ctx());
    build_frame_fixed(fixed, clause, clause_idx, true, static_cast<int>(idx));

    if (entry.action_state == -1) {
        frame_propagation_count_++;
        host_->module_propagate(fixed, frame_action_expr_[clause_idx][idx]);
    } else if (entry.is_conditional && entry.action_state == 1 && entry.cond_state == -1) {
        fixed.push_back(frame_action_expr_[clause_idx][idx]);
        frame_propagation_count_++;
        host_->module_propagate(fixed, frame_cond_expr_[clause_idx][idx]);
    }
}

// ============================================================================
// Variable registration
// ============================================================================

void FrameAxiomModule::register_timestep_variables(int timestep) {
    if (timestep > 0) {
        // Register frame-specific action variables (may overlap with shared;
        // all_registered_ids_ deduplicates via Z3 expr ID).
        const Z3VariableFactory& vf = *shared_->variable_factory;
        if (!shared_->registered_action_vars.contains(timestep - 1)) {
            // Action vars not registered yet — the composite will handle this.
            // But we need them registered for frame role tracking.
            auto vars = vf.get_all_action_variables(timestep - 1);
            if (!vars.empty()) {
                for (const auto& v : vars) {
                    if (all_registered_ids_.insert(v->id()).second)
                        host_->module_add(*v);
                }
            }
        } else {
            // Already in shared state — just ensure they're in our ID set
            for (const auto& v : shared_->registered_action_vars[timestep - 1]) {
                all_registered_ids_.insert(v->id());
            }
        }

        register_frame_variables(timestep - 1);
    }
}

void FrameAxiomModule::register_frame_variables(int t) {
    auto* grounded = dynamic_cast<GroundedEncoder*>(shared_->encoder);
    if (!grounded) return;

    const auto& epc_index = grounded->get_epc_index();
    const Z3VariableFactory& vf = *shared_->variable_factory;
    z3::context& ctx = host_->z3_ctx();

    for (ExprID eid : shared_->problem->grounded_fluents()) {
        auto epc_it = epc_index.find(eid);
        bool has_modifiers = (epc_it != epc_index.end() && !epc_it->second.empty());

        if (!has_modifiers) {
            z3::expr f_t = grounded->convert_expr_id_to_z3(eid, t);
            z3::expr f_t1 = grounded->convert_expr_id_to_z3(eid, t + 1);
            shared_->solver->add(f_t1 == f_t);
            continue;
        }

        const auto& action_effects = epc_it->second;
        bool is_bool = shared_->problem->is_bool_type(eid);
        size_t clause_idx = frame_clauses_.size();

        z3::expr f_t = grounded->convert_expr_id_to_z3(eid, t);
        z3::expr f_t1 = grounded->convert_expr_id_to_z3(eid, t + 1);

        frame_clauses_.emplace_back(t, eid, is_bool);
        auto& clause = frame_clauses_.back();

        frame_fluent_ft_.push_back(f_t);
        frame_fluent_ft1_.push_back(f_t1);

        if (is_bool) {
            frame_eq_bool_.push_back(ctx.bool_val(true));
            if (all_registered_ids_.insert(f_t.id()).second) host_->module_add(f_t);
            frame_var_to_roles_[f_t.id()].push_back({VarRole::FLUENT_T, clause_idx, 0});
            if (all_registered_ids_.insert(f_t1.id()).second) host_->module_add(f_t1);
            frame_var_to_roles_[f_t1.id()].push_back({VarRole::FLUENT_T1, clause_idx, 0});
        } else {
            std::string eq_name = "feq_" + std::to_string(clause_idx);
            z3::expr eq_bool = ctx.bool_const(eq_name.c_str());
            shared_->solver->add(eq_bool == (f_t == f_t1));
            frame_eq_bool_.push_back(eq_bool);
            if (all_registered_ids_.insert(eq_bool.id()).second) host_->module_add(eq_bool);
            frame_var_to_roles_[eq_bool.id()].push_back({VarRole::EQ_BOOL, clause_idx, 0});
        }

        frame_action_expr_.emplace_back();
        frame_cond_expr_.emplace_back();
        auto& actions_vec = frame_action_expr_.back();
        auto& conds_vec = frame_cond_expr_.back();

        for (size_t i = 0; i < action_effects.size(); ++i) {
            const auto& [action, eff_expr] = action_effects[i];

            EPCEntry entry;
            entry.action = action;
            entry.is_conditional = eff_expr->is_conditional();

            z3::expr a = vf.get_action_variable(*action, t);
            actions_vec.push_back(a);

            if (all_registered_ids_.insert(a.id()).second) host_->module_add(a);
            frame_var_to_roles_[a.id()].push_back({VarRole::ACTION, clause_idx, i});

            if (entry.is_conditional) {
                std::string cname = "fc_" + std::to_string(clause_idx) + "_" + std::to_string(i);
                z3::expr cond_reified = ctx.bool_const(cname.c_str());
                z3::expr cond_z3 = grounded->convert_expr_id_to_z3(eff_expr->condition_id(), t);
                shared_->solver->add(cond_reified == cond_z3);
                conds_vec.push_back(cond_reified);
                if (all_registered_ids_.insert(cond_reified.id()).second) host_->module_add(cond_reified);
                frame_var_to_roles_[cond_reified.id()].push_back({VarRole::CONDITION, clause_idx, i});
            } else {
                conds_vec.push_back(ctx.bool_val(true));
            }

            clause.entries.push_back(entry);
        }
    }
}

// ============================================================================
// Recompute derived counters
// ============================================================================

void FrameAxiomModule::recompute_derived(FrameClause& clause) {
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
// Cleanup
// ============================================================================

void FrameAxiomModule::cleanup() {
    auto& stats = Stats::instance();
    stats.set("frame_prop.clauses_created", static_cast<double>(frame_clauses_.size()));
    stats.set("frame_prop.conflicts", static_cast<double>(frame_conflict_count_));
    stats.set("frame_prop.propagations", static_cast<double>(frame_propagation_count_));
    stats.set("frame_prop.on_fixed_calls", static_cast<double>(frame_on_fixed_count_));
    stats.set("frame_prop.on_final_violations", static_cast<double>(frame_final_violation_count_));
    stats.set("frame_prop.vars_registered", static_cast<double>(all_registered_ids_.size()));
}

} // namespace rantanplan
