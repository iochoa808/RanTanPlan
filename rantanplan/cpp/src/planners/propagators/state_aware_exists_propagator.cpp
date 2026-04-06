#include "state_aware_exists_propagator.hpp"
#include "../../config/config.hpp"
#include "../../util/stats.hpp"
#include "../../encoders/z3_variable_factory.hpp"
#include "../../analysis/interference_analysis.hpp"
#include "../../problem/visitors/fluent_collector.hpp"
#include <algorithm>
#include <functional>

namespace rantanplan {

StateAwareExistsPropagator::StateAwareExistsPropagator(
        z3::solver& solver, const Problem& problem, BaseEncoder& encoder)
    : PropagatorStrategy(solver, encoder),
      problem_(&problem),
      variable_factory_(&encoder.get_variable_factory()),
      parallelism_strategy_(encoder.get_parallelism_strategy()),
      interference_analyzer_(parallelism_strategy_->get_interference_analyzer()),
      encoder_nc_(&encoder),
      solver_ptr_(&solver) {
    solver.set("smt.up.persist_clauses", Config::instance().global.persist_clauses);
}

// ---------------------------------------------------------------------------
// Push / Pop
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::on_push() {
    decision_levels_.push_back(trail_.size());
    edge_decision_levels_.push_back(edge_trail_.size());
}

void StateAwareExistsPropagator::on_pop(unsigned num_scopes) {
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
        if (!edge_decision_levels_.empty()) {
            size_t edge_level_start = edge_decision_levels_.back();
            edge_decision_levels_.pop_back();
            while (edge_trail_.size() > edge_level_start) {
                const auto& entry = edge_trail_.back();
                edge_status_[entry.key] = entry.prev_status;
                edge_trail_.pop_back();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// on_fixed: edge-triggered design
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::on_fixed(z3::expr const& ast,
                                          z3::expr const& value) {
    // --- Edge literal ---
    auto edge_it = edge_lit_to_info_.find(ast.id());
    if (edge_it != edge_lit_to_info_.end()) {
        const auto& info = edge_it->second;
        EdgeKey key{info.src, info.tgt, info.timestep};

        EdgeStatus prev = EdgeStatus::UNKNOWN;
        auto st = edge_status_.find(key);
        if (st != edge_status_.end()) prev = st->second;
        edge_trail_.push_back({key, prev});

        if (value.is_true()) {
            edge_status_[key] = EdgeStatus::PRESENT;
            handle_edge_present(info.src, info.tgt, info.timestep);
        } else {
            edge_status_[key] = EdgeStatus::ABSENT;
        }
        return;
    }

    // --- Action variable ---
    if (!value.is_true()) return;
    auto action_info = variable_factory_->get_action_from_variable(ast);
    if (!action_info) return;
    const Action& action = action_info->first;
    int timestep = action_info->second;

    trail_.push_back({action.id(), timestep});
    active_actions_per_timestep_[timestep].insert(action.id());

    build_footprint_index();
    const auto& active = active_actions_per_timestep_[timestep];
    std::vector<int> cycle;
    if (find_cycle_in_active_actions(active, timestep, cycle,
                                     EdgeFilter::PRESENT_ONLY)) {
        report_cycle(cycle, timestep);
    }
}

// ---------------------------------------------------------------------------
// on_final: soundness backstop
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::on_final() {
    build_footprint_index();
    for (const auto& [timestep, active] : active_actions_per_timestep_) {
        if (active.size() < 2) continue;
        std::vector<int> cycle;
        if (find_cycle_in_active_actions(active, timestep, cycle,
                                         EdgeFilter::PRESENT_AND_UNKNOWN)) {
            on_final_cycles_++;
            report_cycle(cycle, timestep);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Timestep variable registration
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::register_timestep_variables(int timestep) {
    PropagatorStrategy::register_timestep_variables(timestep);
    const Z3VariableFactory& vf = *variable_factory_;
    if (timestep == 0) return;

    int t = timestep - 1;
    if (!registered_action_vars_.contains(t)) {
        auto vars = vf.get_all_action_variables(t);
        if (!vars.empty()) {
            registered_action_vars_[t] = std::move(vars);
            for (const auto& v : registered_action_vars_[t]) add(*v);
        }
    }

    // Register edge literals at new timestep for all known SOMETIMES pairs.
    if (t > current_max_timestep_) {
        for (const auto& pair : registered_pairs_)
            register_edge_at_timestep(pair.first, pair.second, t);
        current_max_timestep_ = t;
    }
}

// ---------------------------------------------------------------------------
// Action activation (PDLA hook) — the lazy discovery point
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::on_action_activated(int action_id,
                                                     int max_timestep) {
    build_footprint_index();
    activated_action_ids_.insert(action_id);
    current_max_timestep_ = std::max(current_max_timestep_, max_timestep);

    auto fp_it = potential_interferers_.find(action_id);
    if (fp_it == potential_interferers_.end()) return;

    for (int other_id : fp_it->second) {
        if (!activated_action_ids_.contains(other_id)) continue;
        for (auto [src, tgt] : {std::pair{action_id, other_id},
                                std::pair{other_id, action_id}}) {
            if (!interference_analyzer_->has_interference(src, tgt)) continue;
            auto pk = std::make_pair(src, tgt);
            if (registered_pairs_.contains(pk)) continue;
            if (classify_edge(src, tgt) != EdgeClass::SOMETIMES) continue;
            registered_pairs_.insert(pk);
            for (int t = 0; t <= max_timestep; t++)
                register_edge_at_timestep(src, tgt, t);
        }
    }
}

// ---------------------------------------------------------------------------
// Edge classification (cached)
// ---------------------------------------------------------------------------

StateAwareExistsPropagator::EdgeClass
StateAwareExistsPropagator::classify_edge(int src_id, int tgt_id) {
    auto pk = std::make_pair(src_id, tgt_id);
    auto it = edge_class_cache_.find(pk);
    if (it != edge_class_cache_.end()) return it->second;

    auto* sem = dynamic_cast<const SemanticInterferenceAnalysis*>(
        interference_analyzer_);
    if (sem && sem->get_interference_source(src_id, tgt_id)
                  == InterferenceSource::CONFLICTING_COND_EFFECTS) {
        edge_class_cache_[pk] = EdgeClass::ALWAYS;
        return EdgeClass::ALWAYS;
    }

    const Action& source = problem_->action(src_id);
    const Action& target = problem_->action(tgt_id);
    z3::expr cond = build_condition_z3(source, target, 0).simplify();

    EdgeClass cls;
    if (cond.is_false())      cls = EdgeClass::NEVER;
    else if (cond.is_true())  cls = EdgeClass::ALWAYS;
    else                      cls = EdgeClass::SOMETIMES;

    edge_class_cache_[pk] = cls;
    return cls;
}

// ---------------------------------------------------------------------------
// Condition formula construction
// ---------------------------------------------------------------------------

bool StateAwareExistsPropagator::build_effect_substitution(
        const Action& action, int timestep,
        z3::expr_vector& from, z3::expr_vector& to) {
    std::unordered_map<ExprID, std::vector<const Effect*>> by_fluent;
    std::vector<ExprID> fluent_order;
    for (const Effect& eff : action.effects()) {
        ExprID fid = eff.effect_expression().fluent_id();
        if (by_fluent.find(fid) == by_fluent.end())
            fluent_order.push_back(fid);
        by_fluent[fid].push_back(&eff);
    }
    for (ExprID fid : fluent_order) {
        z3::expr fluent_z3 = encoder_nc_->convert_expr_id_to_z3(fid, timestep);
        z3::expr composed = fluent_z3;
        for (const Effect* eff : by_fluent[fid]) {
            const auto& ee = eff->effect_expression();
            z3::expr v = encoder_nc_->convert_expr_id_to_z3(ee.value_id(), timestep);
            z3::expr nv = fluent_z3;
            switch (ee.kind()) {
                case EffectExpression::Kind::ASSIGN:   nv = v; break;
                case EffectExpression::Kind::INCREASE:  nv = fluent_z3 + v; break;
                case EffectExpression::Kind::DECREASE:  nv = fluent_z3 - v; break;
            }
            if (eff->is_conditional()) {
                z3::expr cond = encoder_nc_->convert_expr_id_to_z3(
                    ee.condition_id(), timestep);
                composed = z3::ite(cond, nv, composed);
            } else {
                composed = nv;
            }
        }
        from.push_back(fluent_z3);
        to.push_back(composed);
    }
    return true;
}

z3::expr StateAwareExistsPropagator::build_check1_z3(
        const Action& source, const Action& target, int timestep,
        const z3::expr_vector& from_s, const z3::expr_vector& to_s) {
    if (!target.has_precondition() || source.effects().empty())
        return ctx().bool_val(false);
    z3::expr target_pre = encoder_nc_->convert_expr_id_to_z3(
        target.precondition_id(), timestep);
    z3::expr substituted = target_pre.substitute(from_s, to_s);
    if (z3::eq(target_pre, substituted)) return ctx().bool_val(false);
    return !substituted;
}

z3::expr StateAwareExistsPropagator::build_check2_z3(
        const Action& source, const Action& target, int timestep,
        const z3::expr_vector& from_s, const z3::expr_vector& to_s) {
    z3::expr_vector from_t(ctx()), to_t(ctx());
    build_effect_substitution(target, timestep, from_t, to_t);

    std::unordered_set<ExprID> affected;
    for (const Effect& e : source.effects())
        affected.insert(e.effect_expression().fluent_id());
    for (const Effect& e : target.effects())
        affected.insert(e.effect_expression().fluent_id());

    z3::expr_vector diffs(ctx());
    for (ExprID fid : affected) {
        z3::expr var = encoder_nc_->convert_expr_id_to_z3(fid, timestep);
        z3::expr happening = var.substitute(from_t, to_t).substitute(from_s, to_s);
        z3::expr sequential = var.substitute(from_s, to_s).substitute(from_t, to_t);
        if (!z3::eq(happening, sequential))
            diffs.push_back(happening != sequential);
    }
    if (diffs.empty()) return ctx().bool_val(false);
    if (diffs.size() == 1) return diffs[0u];
    return z3::mk_or(diffs);
}

z3::expr StateAwareExistsPropagator::build_condition_z3(
        const Action& source, const Action& target, int timestep) {
    z3::expr_vector from_s(ctx()), to_s(ctx());
    build_effect_substitution(source, timestep, from_s, to_s);
    z3::expr c1 = build_check1_z3(source, target, timestep, from_s, to_s);
    z3::expr c2 = build_check2_z3(source, target, timestep, from_s, to_s);
    if (c1.is_false() && c2.is_false()) return ctx().bool_val(false);
    if (c1.is_false()) return c2;
    if (c2.is_false()) return c1;
    if (c1.is_true() || c2.is_true()) return ctx().bool_val(true);
    return c1 || c2;
}

// ---------------------------------------------------------------------------
// Edge literal registration (create + link in one step)
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::register_edge_at_timestep(int src_id,
                                                           int tgt_id,
                                                           int timestep) {
    EdgeKey key{src_id, tgt_id, timestep};
    if (edge_key_to_lit_.contains(key)) return;

    const Action& source = problem_->action(src_id);
    const Action& target = problem_->action(tgt_id);
    z3::expr cond = build_condition_z3(source, target, timestep);

    std::string name = "edge_" + std::to_string(src_id) + "_" +
                       std::to_string(tgt_id) + "_t" + std::to_string(timestep);
    z3::expr edge_lit = ctx().bool_const(name.c_str());
    solver_ptr_->add(edge_lit == cond);
    add(edge_lit);

    edge_status_[key] = EdgeStatus::UNKNOWN;
    edge_lit_to_info_[edge_lit.id()] = {src_id, tgt_id, timestep};
    edge_key_to_lit_.insert({key, edge_lit});
}

// ---------------------------------------------------------------------------
// Edge-triggered cycle detection
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::handle_edge_present(int src, int tgt,
                                                     int timestep) {
    const auto& active = active_actions_per_timestep_[timestep];
    if (!active.contains(src) || !active.contains(tgt)) return;

    // Fast path: 2-cycle
    EdgeKey reverse_key{tgt, src, timestep};
    bool reverse_present = false;
    auto rev_st = edge_status_.find(reverse_key);
    if (rev_st != edge_status_.end()) {
        reverse_present = (rev_st->second == EdgeStatus::PRESENT);
    } else {
        reverse_present = interference_analyzer_->has_interference(tgt, src);
    }

    if (reverse_present) {
        report_cycle({src, tgt}, timestep);
        return;
    }

    // General: DFS for longer cycles
    std::vector<int> cycle;
    if (find_cycle_in_active_actions(active, timestep, cycle,
                                     EdgeFilter::PRESENT_ONLY)) {
        report_cycle(cycle, timestep);
    }
}

void StateAwareExistsPropagator::report_cycle(const std::vector<int>& cycle,
                                              int timestep) {
    cycle_count_++;

    z3::expr_vector justification(ctx());
    for (int nid : cycle)
        justification.push_back(
            variable_factory_->get_action_variable(
                problem_->action(nid), timestep));
    for (size_t i = 0; i < cycle.size(); i++) {
        int from = cycle[i];
        int to = cycle[(i + 1) % cycle.size()];
        EdgeKey key{from, to, timestep};
        auto lit_it = edge_key_to_lit_.find(key);
        if (lit_it != edge_key_to_lit_.end())
            justification.push_back(lit_it->second);
    }

    if (cycle.size() == 2) {
        two_cycle_propagations_++;
        z3::expr target_var = variable_factory_->get_action_variable(
            problem_->action(cycle[1]), timestep);
        z3::expr_vector premises(ctx());
        for (unsigned j = 0; j < justification.size(); ++j) {
            if (!z3::eq(justification[j], target_var))
                premises.push_back(justification[j]);
        }
        propagate(premises, !target_var);
    } else {
        conflict(justification);
    }
}

// ---------------------------------------------------------------------------
// DFS cycle detection
// ---------------------------------------------------------------------------

bool StateAwareExistsPropagator::find_cycle_in_active_actions(
        const std::unordered_set<int>& active, int timestep,
        std::vector<int>& cycle, EdgeFilter filter) {
    if (active.size() < 2) return false;

    std::unordered_set<int> visited, rec_stack;
    std::vector<int> path;

    std::function<bool(int)> dfs = [&](int cur) -> bool {
        visited.insert(cur);
        rec_stack.insert(cur);
        path.push_back(cur);

        auto fp = potential_interferers_.find(cur);
        if (fp != potential_interferers_.end()) {
            for (int other : fp->second) {
                if (!active.contains(other)) continue;

                bool exists = false;
                EdgeKey key{cur, other, timestep};
                auto st = edge_status_.find(key);
                if (st != edge_status_.end()) {
                    switch (st->second) {
                        case EdgeStatus::PRESENT:
                            exists = true; break;
                        case EdgeStatus::ABSENT:
                            edges_skipped_++; exists = false; break;
                        case EdgeStatus::UNKNOWN:
                            exists = (filter == EdgeFilter::PRESENT_AND_UNKNOWN);
                            break;
                    }
                } else {
                    exists = interference_analyzer_->has_interference(cur, other);
                }

                if (exists) {
                    if (rec_stack.contains(other)) {
                        auto it = std::find(path.begin(), path.end(), other);
                        cycle.assign(it, path.end());
                        return true;
                    }
                    if (!visited.contains(other) && dfs(other)) return true;
                }
            }
        }

        rec_stack.erase(cur);
        path.pop_back();
        return false;
    };

    for (int node : active)
        if (!visited.count(node) && dfs(node)) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Footprint index (identical to ExistsPropagator)
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::build_footprint_index() {
    if (footprint_index_built_) return;
    footprint_index_built_ = true;
    std::unordered_map<ExprID, std::vector<int>> fluent_to_actions;
    for (const Action& action : problem_->actions()) {
        int aid = action.id();
        for (const auto& effect : action.effects())
            fluent_to_actions[effect.effect_expression().fluent_id()].push_back(aid);
        if (action.has_precondition()) {
            FluentCollector collector(*problem_);
            collector.collect_from_id(action.precondition_id());
            for (ExprID fid : collector.get_fluents())
                fluent_to_actions[fid].push_back(aid);
        }
    }
    for (const auto& [fid, aids] : fluent_to_actions)
        for (size_t i = 0; i < aids.size(); ++i)
            for (size_t j = i + 1; j < aids.size(); ++j)
                if (aids[i] != aids[j]) {
                    potential_interferers_[aids[i]].push_back(aids[j]);
                    potential_interferers_[aids[j]].push_back(aids[i]);
                }
    for (auto& [aid, nb] : potential_interferers_) {
        std::sort(nb.begin(), nb.end());
        nb.erase(std::unique(nb.begin(), nb.end()), nb.end());
    }
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

void StateAwareExistsPropagator::cleanup() {
    auto& stats = Stats::instance();
    stats.set("propagator.exists_total_cycles", cycle_count_);
    stats.set("propagator.sa_edges_skipped", edges_skipped_);
    stats.set("propagator.sa_registered_pairs",
              static_cast<int>(registered_pairs_.size()));
    stats.set("propagator.sa_edge_lits",
              static_cast<int>(edge_key_to_lit_.size()));
    stats.set("propagator.sa_two_cycle_propagations", two_cycle_propagations_);
    stats.set("propagator.sa_on_final_cycles", on_final_cycles_);
}

} // namespace rantanplan
