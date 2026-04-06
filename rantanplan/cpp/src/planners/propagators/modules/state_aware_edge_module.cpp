#include "state_aware_edge_module.hpp"
#include "../../../util/stats.hpp"
#include "../../../util/logger.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <queue>

namespace rantanplan {

// ---------------------------------------------------------------------------
// Push / Pop (private edge trail)
// ---------------------------------------------------------------------------

void StateAwareEdgeModule::on_push() {
    edge_decision_levels_.push_back(edge_trail_.size());
}

void StateAwareEdgeModule::on_pop(unsigned num_scopes) {
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (edge_decision_levels_.empty()) continue;
        size_t edge_level_start = edge_decision_levels_.back();
        edge_decision_levels_.pop_back();
        while (edge_trail_.size() > edge_level_start) {
            const auto& entry = edge_trail_.back();
            edge_status_[entry.key] = entry.prev_status;
            edge_trail_.pop_back();
        }
    }
}

// ---------------------------------------------------------------------------
// on_fixed
// ---------------------------------------------------------------------------

void StateAwareEdgeModule::on_fixed(const z3::expr& ast, const z3::expr& value) {
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

    // --- Action variable: DFS with PRESENT_ONLY ---
    if (!value.is_true()) return;
    auto action_info = shared_->variable_factory->get_action_from_variable(ast);
    if (!action_info) return;
    int timestep = action_info->second;

    shared_->build_footprint_index();
    const auto& active = shared_->active_actions_per_timestep[timestep];
    std::vector<int> cycle;
    if (find_cycle_in_active_actions(active, timestep, cycle,
                                     EdgeFilter::PRESENT_ONLY)) {
        report_cycle(cycle, timestep);
    }
}

// ---------------------------------------------------------------------------
// on_final: soundness backstop
// ---------------------------------------------------------------------------

void StateAwareEdgeModule::on_final() {
    shared_->build_footprint_index();
    for (const auto& [timestep, active] : shared_->active_actions_per_timestep) {
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
// Action activation (PDLA hook)
// ---------------------------------------------------------------------------

void StateAwareEdgeModule::on_action_activated(int action_id, int max_timestep) {
    shared_->build_footprint_index();
    activated_action_ids_.insert(action_id);
    current_max_timestep_ = std::max(current_max_timestep_, max_timestep);

    auto fp_it = shared_->potential_interferers.find(action_id);
    if (fp_it == shared_->potential_interferers.end()) return;

    for (int other_id : fp_it->second) {
        if (!activated_action_ids_.contains(other_id)) continue;
        for (auto [src, tgt] : {std::pair{action_id, other_id},
                                std::pair{other_id, action_id}}) {
            if (!shared_->interference->has_interference(src, tgt)) continue;
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
// Timestep variable registration
// ---------------------------------------------------------------------------

void StateAwareEdgeModule::register_timestep_variables(int timestep) {
    if (timestep == 0) return;
    int t = timestep - 1;

    // Register edge literals at new timestep for all known SOMETIMES pairs.
    if (t > current_max_timestep_) {
        for (const auto& pair : registered_pairs_)
            register_edge_at_timestep(pair.first, pair.second, t);
        current_max_timestep_ = t;
    }
}

// ---------------------------------------------------------------------------
// Edge classification
// ---------------------------------------------------------------------------

StateAwareEdgeModule::EdgeClass
StateAwareEdgeModule::classify_edge(int src_id, int tgt_id) {
    auto pk = std::make_pair(src_id, tgt_id);
    auto it = edge_class_cache_.find(pk);
    if (it != edge_class_cache_.end()) return it->second;

    auto* sem = dynamic_cast<const SemanticInterferenceAnalysis*>(shared_->interference);
    if (sem && sem->get_interference_source(src_id, tgt_id)
                  == InterferenceSource::CONFLICTING_COND_EFFECTS) {
        edge_class_cache_[pk] = EdgeClass::ALWAYS;
        return EdgeClass::ALWAYS;
    }

    const Action& source = shared_->problem->action(src_id);
    const Action& target = shared_->problem->action(tgt_id);
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

bool StateAwareEdgeModule::build_effect_substitution(
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
        z3::expr fluent_z3 = shared_->encoder->convert_expr_id_to_z3(fid, timestep);
        z3::expr composed = fluent_z3;
        for (const Effect* eff : by_fluent[fid]) {
            const auto& ee = eff->effect_expression();
            z3::expr v = shared_->encoder->convert_expr_id_to_z3(ee.value_id(), timestep);
            z3::expr nv = fluent_z3;
            switch (ee.kind()) {
                case EffectExpression::Kind::ASSIGN:   nv = v; break;
                case EffectExpression::Kind::INCREASE:  nv = fluent_z3 + v; break;
                case EffectExpression::Kind::DECREASE:  nv = fluent_z3 - v; break;
            }
            if (eff->is_conditional()) {
                z3::expr cond = shared_->encoder->convert_expr_id_to_z3(
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

z3::expr StateAwareEdgeModule::build_check1_z3(
        const Action& source, const Action& target, int timestep,
        const z3::expr_vector& from_s, const z3::expr_vector& to_s) {
    z3::context& ctx = host_->z3_ctx();
    if (!target.has_precondition() || source.effects().empty())
        return ctx.bool_val(false);
    z3::expr target_pre = shared_->encoder->convert_expr_id_to_z3(
        target.precondition_id(), timestep);
    z3::expr substituted = target_pre.substitute(from_s, to_s);
    if (z3::eq(target_pre, substituted)) return ctx.bool_val(false);
    return !substituted;
}

z3::expr StateAwareEdgeModule::build_check2_z3(
        const Action& source, const Action& target, int timestep,
        const z3::expr_vector& from_s, const z3::expr_vector& to_s) {
    z3::context& ctx = host_->z3_ctx();
    z3::expr_vector from_t(ctx), to_t(ctx);
    build_effect_substitution(target, timestep, from_t, to_t);

    std::unordered_set<ExprID> affected;
    for (const Effect& e : source.effects())
        affected.insert(e.effect_expression().fluent_id());
    for (const Effect& e : target.effects())
        affected.insert(e.effect_expression().fluent_id());

    z3::expr_vector diffs(ctx);
    for (ExprID fid : affected) {
        z3::expr var = shared_->encoder->convert_expr_id_to_z3(fid, timestep);
        z3::expr happening = var.substitute(from_t, to_t).substitute(from_s, to_s);
        z3::expr sequential = var.substitute(from_s, to_s).substitute(from_t, to_t);
        if (!z3::eq(happening, sequential))
            diffs.push_back(happening != sequential);
    }
    if (diffs.empty()) return ctx.bool_val(false);
    if (diffs.size() == 1) return diffs[0u];
    return z3::mk_or(diffs);
}

z3::expr StateAwareEdgeModule::build_condition_z3(
        const Action& source, const Action& target, int timestep) {
    z3::context& ctx = host_->z3_ctx();
    z3::expr_vector from_s(ctx), to_s(ctx);
    build_effect_substitution(source, timestep, from_s, to_s);
    z3::expr c1 = build_check1_z3(source, target, timestep, from_s, to_s);
    z3::expr c2 = build_check2_z3(source, target, timestep, from_s, to_s);
    if (c1.is_false() && c2.is_false()) return ctx.bool_val(false);
    if (c1.is_false()) return c2;
    if (c2.is_false()) return c1;
    if (c1.is_true() || c2.is_true()) return ctx.bool_val(true);
    return c1 || c2;
}

// ---------------------------------------------------------------------------
// Edge literal registration
// ---------------------------------------------------------------------------

void StateAwareEdgeModule::register_edge_at_timestep(int src_id, int tgt_id,
                                                     int timestep) {
    EdgeKey key{src_id, tgt_id, timestep};
    if (edge_key_to_lit_.contains(key)) return;

    z3::context& ctx = host_->z3_ctx();
    const Action& source = shared_->problem->action(src_id);
    const Action& target = shared_->problem->action(tgt_id);
    z3::expr cond = build_condition_z3(source, target, timestep);

    std::string name = "edge_" + std::to_string(src_id) + "_" +
                       std::to_string(tgt_id) + "_t" + std::to_string(timestep);
    z3::expr edge_lit = ctx.bool_const(name.c_str());
    shared_->solver->add(edge_lit == cond);
    host_->module_add(edge_lit);

    edge_status_[key] = EdgeStatus::UNKNOWN;
    edge_lit_to_info_[edge_lit.id()] = {src_id, tgt_id, timestep};
    edge_key_to_lit_.insert({key, edge_lit});
}

// ---------------------------------------------------------------------------
// Edge-triggered cycle detection
// ---------------------------------------------------------------------------

void StateAwareEdgeModule::handle_edge_present(int src, int tgt, int timestep) {
    const auto& active = shared_->active_actions_per_timestep[timestep];
    if (!active.contains(src) || !active.contains(tgt)) return;

    // Fast path: 2-cycle
    EdgeKey reverse_key{tgt, src, timestep};
    bool reverse_present = false;
    auto rev_st = edge_status_.find(reverse_key);
    if (rev_st != edge_status_.end()) {
        reverse_present = (rev_st->second == EdgeStatus::PRESENT);
    } else {
        reverse_present = shared_->interference->has_interference(tgt, src);
    }

    if (reverse_present) {
        report_cycle({src, tgt}, timestep);
        return;
    }

    std::vector<int> cycle;
    if (find_cycle_in_active_actions(active, timestep, cycle,
                                     EdgeFilter::PRESENT_ONLY)) {
        report_cycle(cycle, timestep);
    }
}

void StateAwareEdgeModule::report_cycle(const std::vector<int>& cycle,
                                        int timestep) {
    cycle_count_++;
    z3::context& ctx = host_->z3_ctx();

    z3::expr_vector justification(ctx);
    for (int nid : cycle)
        justification.push_back(
            shared_->variable_factory->get_action_variable(
                shared_->problem->action(nid), timestep));
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
        z3::expr target_var = shared_->variable_factory->get_action_variable(
            shared_->problem->action(cycle[1]), timestep);
        z3::expr_vector premises(ctx);
        for (unsigned j = 0; j < justification.size(); ++j) {
            if (!z3::eq(justification[j], target_var))
                premises.push_back(justification[j]);
        }
        host_->module_propagate(premises, !target_var);
    } else {
        host_->module_conflict(justification);
    }
}

bool StateAwareEdgeModule::find_cycle_in_active_actions(
        const std::unordered_set<int>& active, int timestep,
        std::vector<int>& cycle, EdgeFilter filter) {
    if (active.size() < 2) return false;

    std::unordered_set<int> visited, rec_stack;
    std::vector<int> path;

    std::function<bool(int)> dfs = [&](int cur) -> bool {
        visited.insert(cur);
        rec_stack.insert(cur);
        path.push_back(cur);

        auto fp = shared_->potential_interferers.find(cur);
        if (fp != shared_->potential_interferers.end()) {
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
                    exists = shared_->interference->has_interference(cur, other);
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
// Cleanup
// ---------------------------------------------------------------------------

std::vector<const Action*> StateAwareEdgeModule::serialize_actions(
        int timestep, const std::vector<const Action*>& actions) const {
    if (actions.size() <= 1) return actions;

    bool debug = false;

    std::unordered_map<int, std::vector<int>> adj;
    std::unordered_map<int, int> in_degree;
    std::unordered_set<int> ids;

    for (const Action* a : actions) {
        int id = a->id();
        ids.insert(id);
        in_degree[id];
    }

    z3::model model = shared_->solver->get_model();

    if (debug) {
        std::cout << "\n[SA-serialize] t=" << timestep
                  << " actions=" << actions.size() << ":\n";
        for (const Action* a : actions)
            std::cout << "  [" << a->id() << "] " << a->name() << "\n";
    }

    for (const Action* a : actions) {
        for (const Action* b : actions) {
            if (a == b) continue;
            int src = a->id(), tgt = b->id();

            bool interferes = false;
            EdgeKey key{src, tgt, timestep};
            auto lit_it = edge_key_to_lit_.find(key);

            // Classification for debug
            std::string source_str = "none";
            std::string detail;

            if (lit_it != edge_key_to_lit_.end()) {
                z3::expr val = model.eval(lit_it->second, true);
                interferes = val.is_true();
                source_str = "edge_lit";

                if (debug) {
                    // Also show the trail status for comparison
                    auto trail_st = edge_status_.find(key);
                    std::string trail_str = "no_entry";
                    if (trail_st != edge_status_.end()) {
                        switch (trail_st->second) {
                            case EdgeStatus::PRESENT: trail_str = "PRESENT"; break;
                            case EdgeStatus::ABSENT:  trail_str = "ABSENT"; break;
                            case EdgeStatus::UNKNOWN: trail_str = "UNKNOWN"; break;
                        }
                    }

                    // Show the edge classification
                    auto pk = std::make_pair(src, tgt);
                    auto cls_it = edge_class_cache_.find(pk);
                    std::string cls_str = "?";
                    if (cls_it != edge_class_cache_.end()) {
                        switch (cls_it->second) {
                            case EdgeClass::UNCHECKED: cls_str = "UNCHECKED"; break;
                            case EdgeClass::NEVER:     cls_str = "NEVER"; break;
                            case EdgeClass::ALWAYS:    cls_str = "ALWAYS"; break;
                            case EdgeClass::SOMETIMES: cls_str = "SOMETIMES"; break;
                        }
                    }

                    // Evaluate the condition formula directly
                    const Action& source_a = shared_->problem->action(src);
                    const Action& target_a = shared_->problem->action(tgt);
                    z3::expr cond = const_cast<StateAwareEdgeModule*>(this)->build_condition_z3(source_a, target_a, timestep);
                    z3::expr cond_val = model.eval(cond, true);

                    detail = " trail=" + trail_str +
                             " class=" + cls_str +
                             " lit=" + val.to_string() +
                             " cond=" + cond_val.to_string();
                }
            } else {
                interferes = shared_->interference->has_interference(src, tgt);
                source_str = "static";

                if (debug) {
                    // Check classification
                    auto pk = std::make_pair(src, tgt);
                    auto cls_it = edge_class_cache_.find(pk);
                    std::string cls_str = "not_cached";
                    if (cls_it != edge_class_cache_.end()) {
                        switch (cls_it->second) {
                            case EdgeClass::UNCHECKED: cls_str = "UNCHECKED"; break;
                            case EdgeClass::NEVER:     cls_str = "NEVER"; break;
                            case EdgeClass::ALWAYS:    cls_str = "ALWAYS"; break;
                            case EdgeClass::SOMETIMES: cls_str = "SOMETIMES"; break;
                        }
                    }
                    detail = " class=" + cls_str +
                             " has_interf=" + std::string(interferes ? "T" : "F");
                }
            }

            if (debug) {
                std::string mark = interferes ? "EDGE" : "    ";
                std::cout << "  " << mark << " " << a->name() << " -> " << b->name()
                          << " [" << source_str << detail << "]\n";
            }

            if (interferes) {
                adj[tgt].push_back(src);
                in_degree[src]++;
            }
        }
    }

    // Kahn's algorithm for topological sort (execution order)
    std::queue<int> ready;
    for (int id : ids) {
        if (in_degree[id] == 0) ready.push(id);
    }

    std::unordered_map<int, const Action*> id_to_action;
    for (const Action* a : actions) id_to_action[a->id()] = a;

    std::vector<const Action*> result;
    result.reserve(actions.size());

    while (!ready.empty()) {
        int cur = ready.front();
        ready.pop();
        result.push_back(id_to_action[cur]);
        for (int next : adj[cur]) {
            if (--in_degree[next] == 0) ready.push(next);
        }
    }

    // If cycle detected (shouldn't happen after SAT), return what we have
    // plus remaining actions in arbitrary order.
    if (result.size() < actions.size()) {
        if (debug) {
            std::cout << "  WARNING: cycle in serialization graph, "
                      << result.size() << "/" << actions.size() << " ordered\n";
        }
        std::unordered_set<int> added;
        for (const Action* a : result) added.insert(a->id());
        for (const Action* a : actions) {
            if (!added.contains(a->id())) result.push_back(a);
        }
    }

    if (debug) {
        std::cout << "  ORDER:";
        for (const Action* a : result) std::cout << " " << a->name();
        std::cout << "\n";
    }

    return result;
}

void StateAwareEdgeModule::cleanup() {
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
