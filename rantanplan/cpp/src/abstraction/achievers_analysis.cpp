#include "achievers_analysis.hpp"
#include "../analysis/numeric_relaxed_planning_graph.hpp"
#include "../util/ipar_names.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/logger.hpp"
#include <iostream>
#include <algorithm>

namespace rantanplan {

AchieversAnalysis::AchieversAnalysis(const Problem& problem)
    : problem_(&problem) {
    // Build our own NumericRelaxedPlanningGraph internally.
    auto start_rpg = std::chrono::high_resolution_clock::now();

    NumericRelaxedPlanningGraph rpg(problem);
    rpg.set_stop_when_all_reachable(false);  // Run to fixpoint for widest bounds
    rpg.build();

    RPGData data;
    data.state_variable_bounds = rpg.get_state_variable_bounds();
    data.action_first_layers = rpg.get_action_first_layers();
    data.layer_count = static_cast<int>(rpg.get_layer_count());

    auto end_rpg = std::chrono::high_resolution_clock::now();
    double rpg_time = std::chrono::duration<double>(end_rpg - start_rpg).count();
    Stats::instance().set("achievers_analysis.rpg_time_seconds", rpg_time);
    Stats::instance().set("achievers_analysis.rpg_goal_reachable", rpg.are_goals_achievable() ? 1 : 0);

    Logger::instance().component(VerbosityLevel::INFO, "Achievers", {
        {"RPG", std::to_string(rpg_time) + "s"},
        {"mem", std::to_string(static_cast<int>(MemoryTracker::instance().get_current_memory_mb())) + "MB"}
    });

    initialize_from_rpg_data(problem, std::move(data));
}

AchieversAnalysis::AchieversAnalysis(const Problem& problem, RPGData rpg_data)
    : problem_(&problem) {
    Logger::instance().component(VerbosityLevel::INFO, "Achievers", {
        {"RPG", "pre-computed"},
        {"bounds", std::to_string(rpg_data.state_variable_bounds.size())},
        {"layers", std::to_string(rpg_data.layer_count)}
    });

    initialize_from_rpg_data(problem, std::move(rpg_data));
}

void AchieversAnalysis::initialize_from_rpg_data(const Problem& problem, RPGData rpg_data) {
    auto& stats = Stats::instance();
    auto start_total = std::chrono::high_resolution_clock::now();

    // Initialize SMT infrastructure
    ctx_ = std::make_unique<z3::context>();
    variable_factory_ = std::make_unique<Z3VariableFactory>(*ctx_);
    variable_factory_->set_problem(problem_);
    visitor_ = std::make_unique<GroundedEncodingVisitor>(*ctx_, problem_, variable_factory_.get());
    persistent_solver_ = std::make_unique<z3::solver>(*ctx_);

    // Store RPG data
    state_variable_bounds_ = std::move(rpg_data.state_variable_bounds);
    action_id_to_first_layer_ = std::move(rpg_data.action_first_layers);
    arpg_num_layers_ = rpg_data.layer_count;

    stats.set("achievers_analysis.state_variable_bounds_count",
              static_cast<double>(state_variable_bounds_.size()));

    // Initialize persistent solver with bounds constraints
    initialize_persistent_solver();

    // Run semantic analysis
    auto start_analysis = std::chrono::high_resolution_clock::now();
    analyze(problem);
    auto end_analysis = std::chrono::high_resolution_clock::now();

    double analysis_time = std::chrono::duration<double>(end_analysis - start_analysis).count();

    stats.set("achievers_analysis.semantic_analysis_time_seconds", analysis_time);

    auto end_total = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double>(end_total - start_total).count();

    Logger::instance().component(VerbosityLevel::INFO, "Achievers", {
        {"semantic", std::to_string(analysis_time) + "s"},
        {"total", std::to_string(total_time) + "s"},
        {"mem", std::to_string(static_cast<int>(MemoryTracker::instance().get_current_memory_mb())) + "MB"}
    });

    stats.set("achievers_analysis.total_time_seconds", total_time);
}

void AchieversAnalysis::clear() {
    precondition_to_actions_.clear();
    action_to_preconditions_.clear();
    condition_to_achievers_.clear();
    action_to_achieved_conditions_.clear();
    pre_conditions_.clear();
    goal_conditions_.clear();

    // Invalidate cache
    all_conditions_cached_ = false;
    all_conditions_cache_.clear();

    // Note: achiever_relevant_fluents_ and changed_bound_fluents_ are NOT
    // cleared here — they persist across analyze() calls for incremental reuse.
}

void AchieversAnalysis::analyze(const Problem& problem) {
    auto& stats = Stats::instance();
    clear();

    // Process all actions for precondition analysis
    for (const auto& action : problem.actions()) {
        process_action_preconditions(action);
    }
    stats.set("achievers_analysis.total_preconditions", precondition_to_actions_.size());

    // Process goal conditions
    for (size_t i = 0; i < problem.goal_count(); ++i) {
        process_goal_conditions(problem.goal(i));
    }
    stats.set("achievers_analysis.total_goal_conditions", goal_conditions_.size());
    stats.set("achievers_analysis.total_actions", problem.actions().size());

    // Register conditional-effect guards so the SMT pass computes their achievers.
    // condition_to_achievers_ is otherwise built only from preconditions and goals, but
    // a `when` guard is a real dependency too — goal-relevance pruning must know who can
    // make it true, or it deletes the only action that can:
    //   (:action activate :effect (flag-b))
    //   (:action process  :effect (when (flag-b) (assign (cell-b) 0)))   goal: cell-b = 0
    // process achieves the goal and has no preconditions, so the backward walk stops there
    // and prunes activate, leaving the goal unreachable. Only bites when a guard appears
    // nowhere else; in most domains it is also some action's precondition and gets
    // registered for free. Consumed by compute_goal_relevant_action_indices() below —
    // neither half works alone. Test: xts/benchmarks/unit/multi_when.
    for (const auto& action : problem.actions()) {
        for (const Effect& eff : action.effects()) {
            if (!eff.is_conditional()) continue;
            std::vector<ExprID> cond_literals;
            extract_cnf_literals(eff.effect_expression().condition_id(), cond_literals);
            for (ExprID eid : cond_literals) {
                if (!condition_to_achievers_.contains(eid)) {
                    condition_to_achievers_[eid] = std::unordered_set<Action>();
                }
            }
        }
    }

    // Perform semantic achiever analysis using SMT
    analyze_semantic_achievers();
}

void AchieversAnalysis::process_action_preconditions(const Action& action) {
    if (!action.has_precondition()) return;

    // Extract CNF literals from the precondition
    std::vector<ExprID> literals;
    extract_cnf_literals(action.precondition_id(), literals);

    // Map each literal to this action (using ExprID keys)
    for (ExprID eid : literals) {
        precondition_to_actions_[eid].insert(action);
        action_to_preconditions_[action].insert(eid);
        pre_conditions_.insert(eid);
    }
}

void AchieversAnalysis::process_goal_conditions(const Goal& goal) {
    // Extract CNF literals from the goal expression
    std::vector<ExprID> literals;
    extract_cnf_literals(goal.goal_id(), literals);

    // Store goal conditions and ensure they're tracked in achiever analysis
    for (ExprID eid : literals) {
        goal_conditions_.insert(eid);

        // Ensure it's in our condition_to_achievers_ map for analysis
        if (!condition_to_achievers_.contains(eid)) {
            condition_to_achievers_[eid] = std::unordered_set<Action>();
        }
    }
}

// Extract literals from CNF expressions for precondition analysis.
// Since we use semantic SMT-based analysis for achievers, we can handle disjunctions
// naturally by preserving OR expressions as single literals. The SMT solver will
// properly evaluate disjunctive conditions without needing to split them.
void AchieversAnalysis::extract_cnf_literals(ExprID eid, std::vector<ExprID>& literals) const {
    const auto& pool = problem_->pool();
    if (pool.is_and(eid)) {
        // AND expression - extract literals from each conjunct recursively
        for (ExprID child : pool.arguments(eid)) {
            extract_cnf_literals(child, literals);
        }
    } else {
        // Base case: this is a literal (atomic or disjunctive)
        if (!pool.is_fluent_symbol(eid) && !pool.is_function_symbol(eid)) {
            literals.push_back(eid);
        }
    }
}

const std::unordered_set<Action>& AchieversAnalysis::get_actions_requiring_precondition(ExprID precondition) const {
    auto it = precondition_to_actions_.find(precondition);
    if (it != precondition_to_actions_.end()) {
        return it->second;
    }
    static const std::unordered_set<Action> empty_set;
    return empty_set;
}


void AchieversAnalysis::analyze_semantic_achievers() {
    auto& stats = Stats::instance();
    z3_query_count_ = 0;

    bool incremental = !achiever_relevant_fluents_.empty();
    size_t cache_skip_count = 0;
    size_t cache_adopt_count = 0;

    // Get all conditions from preconditions and goals (both Boolean and numeric)
    std::unordered_set<ExprID> all_conditions;

    // Collect all preconditions
    for (const auto& [condition_eid, actions] : precondition_to_actions_) {
        all_conditions.insert(condition_eid);
    }

    // Collect all goal conditions
    for (const auto& goal_eid : goal_conditions_) {
        all_conditions.insert(goal_eid);
    }

    // Collect conditional-effect conditions registered by analyze() — these are
    // conditions that appear only as effect guards (not as preconditions or goals)
    // and must have their achievers computed so the BFS doesn't prune their enablers.
    for (const auto& [condition_eid, _] : condition_to_achievers_) {
        all_conditions.insert(condition_eid);
    }

    stats.set("achievers_analysis.conditions_to_analyze", all_conditions.size());

    Logger::instance().component(VerbosityLevel::INFO, "Achievers", {
        {"conditions", std::to_string(all_conditions.size())},
        {"actions", std::to_string(problem_->action_count())},
        {"incremental", incremental ? "yes" : "no"}
    });

    // For each condition, check which actions can achieve it semantically
    for (ExprID condition_eid : all_conditions) {
        auto condition_fluents = collect_fluents_in_expression(condition_eid);

        for (const auto& action : problem_->actions()) {
            auto action_fluents = get_action_modified_fluents(action);

            // For non-boolean conditions, always try SMT checking since fluent collection may miss nested fluents
            bool should_test = !condition_fluents.empty() && fluent_sets_intersect(condition_fluents, action_fluents);

            // For numeric conditions where fluent collection failed, still try if action has numeric effects
            if (!should_test && problem_->pool().is_function_application(condition_eid) && condition_fluents.empty()) {
                should_test = !action_fluents.empty();
            }

            if (!should_test) {
                continue;
            }

            // --- Incremental cache logic ---
            if (incremental) {
                auto cond_it = achiever_relevant_fluents_.find(condition_eid);
                bool was_achiever = false;
                if (cond_it != achiever_relevant_fluents_.end()) {
                    auto label_it = cond_it->second.find(action.label());
                    was_achiever = (label_it != cond_it->second.end());

                    if (was_achiever) {
                        // Check if any relevant fluent's bounds changed
                        bool needs_reverify = false;
                        for (ExprID f : label_it->second) {
                            if (changed_bound_fluents_.count(f)) {
                                needs_reverify = true;
                                break;
                            }
                        }

                        if (!needs_reverify) {
                            // Same bounds for all relevant fluents → adopt
                            condition_to_achievers_[condition_eid].insert(action);
                            action_to_achieved_conditions_[action].insert(condition_eid);
                            ++cache_adopt_count;
                            continue;
                        }

                        // Bounds changed → re-verify with Z3 (falls through)
                    }
                }

                if (!was_achiever) {
                    // Was UNSAT with wider bounds → still UNSAT
                    ++cache_skip_count;
                    continue;
                }
            }

            // Z3 check (first run, or re-verification of previous achiever)
            if (check_action_achieves_condition_with_pushpop(action, condition_eid)) {
                condition_to_achievers_[condition_eid].insert(action);
                action_to_achieved_conditions_[action].insert(condition_eid);
                // Store relevant fluents for future incremental use
                achiever_relevant_fluents_[condition_eid][action.label()] =
                    collect_query_relevant_fluents(action, condition_eid);
            } else if (incremental) {
                // Was achiever but now UNSAT → remove from cache
                achiever_relevant_fluents_[condition_eid].erase(action.label());
            }
        }
    }

    // Clean up empty entries in the relevant fluents cache
    if (incremental) {
        for (auto it = achiever_relevant_fluents_.begin();
             it != achiever_relevant_fluents_.end(); ) {
            if (it->second.empty()) {
                it = achiever_relevant_fluents_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Record final statistics
    stats.set("achievers_analysis.z3_queries_count", z3_query_count_);
    stats.set("achievers_analysis.cache_skip_count", static_cast<double>(cache_skip_count));
    stats.set("achievers_analysis.cache_adopt_count", static_cast<double>(cache_adopt_count));
    stats.set("achievers_analysis.total_conditions", condition_to_achievers_.size());
    stats.set("achievers_analysis.total_achievable_conditions",
              std::count_if(condition_to_achievers_.begin(), condition_to_achievers_.end(),
                           [](const auto& pair) { return !pair.second.empty(); }));
    stats.set("achievers_analysis.total_actions_with_achievers", action_to_achieved_conditions_.size());

    if (incremental) {
        Logger::instance().component(VerbosityLevel::INFO, "Achievers", {
            {"cache_skips", std::to_string(cache_skip_count)},
            {"cache_adopts", std::to_string(cache_adopt_count)},
            {"z3_queries", std::to_string(z3_query_count_)}
        });
    }
}

const std::unordered_set<ExprID>& AchieversAnalysis::get_achieved_conditions(const Action& action) const {
    auto it = action_to_achieved_conditions_.find(action);
    if (it != action_to_achieved_conditions_.end()) {
        return it->second;
    }
    static const std::unordered_set<ExprID> empty_set;
    return empty_set;
}

const std::unordered_set<Action>& AchieversAnalysis::get_achievers(ExprID condition) const {
    auto it = condition_to_achievers_.find(condition);
    if (it != condition_to_achievers_.end()) {
        return it->second;
    }
    static const std::unordered_set<Action> empty_set;
    return empty_set;
}

const std::unordered_set<ExprID>& AchieversAnalysis::get_preconditions(const Action& action) const {
    auto it = action_to_preconditions_.find(action);
    if (it != action_to_preconditions_.end()) {
        return it->second;
    }
    static const std::unordered_set<ExprID> empty_set;
    return empty_set;
}

const std::unordered_set<ExprID>& AchieversAnalysis::get_all_conditions() const {
    // Return cached result if available
    if (all_conditions_cached_) {
        return all_conditions_cache_;
    }

    // Build the cache
    all_conditions_cache_.clear();

    // Add conditions in all actions
    for (const auto& [precondition_eid, actions] : precondition_to_actions_) {
        all_conditions_cache_.insert(precondition_eid);
    }

    // Add all goal conditions
    for (const auto& goal_eid : goal_conditions_) {
        all_conditions_cache_.insert(goal_eid);
    }

    all_conditions_cached_ = true;
    return all_conditions_cache_;
}

const std::unordered_set<ExprID>& AchieversAnalysis::get_pre_conditions() const {
    return pre_conditions_;
}

const std::unordered_set<ExprID>& AchieversAnalysis::get_goal_conditions() const {
    return goal_conditions_;
}

std::unordered_set<ExprID> AchieversAnalysis::collect_fluents_in_expression(ExprID eid) {
    FluentCollector collector(*problem_);
    collector.collect_from_id(eid);
    return collector.get_fluents();
}

std::unordered_set<ExprID> AchieversAnalysis::get_action_modified_fluents(const Action& action) {
    std::unordered_set<ExprID> modified_fluents;
    const ExprPool& pool = problem_->pool();

    // [XTS] Build lookup: "base[k]" name → ExprID for IPAR cell SVs in grounded_fluents.
    // ARRAY_WRITE(base_sv, k) effects must also be considered as modifying the
    // corresponding IPAR cell SV "base[k]", so that the fluent-intersection check
    // correctly detects that reset_all achieves the goal cells[0]=0.
    std::unordered_map<std::string, ExprID> ipar_cell_sv_by_name;
    for (ExprID gf : problem_->grounded_fluents()) {
        if (!pool.is_state_variable(gf)) continue;
        if (pool.argument_count(gf) != 0) continue;
        ExprID h = pool.head_symbol_id(gf);
        if (!pool.payload_is_string(h)) continue;
        const std::string& nm = pool.payload_string(h);
        if (nm.find('[') != std::string::npos) ipar_cell_sv_by_name.emplace(nm, gf);
    }

    for (const auto& effect_wrapper : action.effects()) {
        const EffectExpression& effect = effect_wrapper.effect_expression();
        ExprID eid = effect.fluent_id();
        if (eid.valid()) {
            modified_fluents.insert(eid);
            // For ARRAY_WRITE effects, also mark the base SV as modified.
            // Walk through ARRAY_WRITE chains; if the chain ends in an ARRAY_READ
            // (2D write case: ARRAY_WRITE(ARRAY_READ(SV(board), i), j)),
            // continue into the ARRAY_READ to find the base SV.
            // [XTS] For an N-D cell write with all-constant indices, also mark the
            // matching IPAR cell SV ("base[i0][i1]...") as modified so the achiever
            // intersection check can link this effect to goals that reference cell SVs.
            // Indices are collected innermost-first while peeling (the ARRAY_WRITE
            // node carries the outermost index), then reversed for the name.
            if (pool.is_function_application(eid) &&
                pool.op(eid) == ExprOperator::ARRAY_WRITE &&
                pool.argument_count(eid) >= 2) {
                std::vector<int64_t> rev_idxs;
                bool all_const = true;
                ExprID idx_arg = pool.argument(eid, 1);
                if (pool.is_constant(idx_arg) && pool.payload_is_int(idx_arg)) {
                    rev_idxs.push_back(pool.payload_int(idx_arg));
                } else {
                    all_const = false;
                }
                ExprID cell_base = pool.argument(eid, 0);
                while (all_const && pool.is_function_application(cell_base) &&
                       pool.op(cell_base) == ExprOperator::ARRAY_READ &&
                       pool.argument_count(cell_base) >= 2) {
                    ExprID inner_idx = pool.argument(cell_base, 1);
                    if (pool.is_constant(inner_idx) && pool.payload_is_int(inner_idx)) {
                        rev_idxs.push_back(pool.payload_int(inner_idx));
                    } else {
                        all_const = false;
                    }
                    cell_base = pool.argument(cell_base, 0);
                }
                if (all_const && pool.is_state_variable(cell_base)) {
                    ExprID bh = pool.head_symbol_id(cell_base);
                    if (pool.payload_is_string(bh)) {
                        std::vector<int64_t> idxs(rev_idxs.rbegin(), rev_idxs.rend());
                        auto it = ipar_cell_sv_by_name.find(
                            make_ipar_cell_name(pool.payload_string(bh), idxs));
                        if (it != ipar_cell_sv_by_name.end())
                            modified_fluents.insert(it->second);
                    }
                }
            }

            ExprID base = eid;
            while (pool.is_function_application(base) &&
                   pool.op(base) == ExprOperator::ARRAY_WRITE &&
                   pool.argument_count(base) >= 1) {
                base = pool.argument(base, 0);
                modified_fluents.insert(base);
            }
            // Peel any remaining ARRAY_READ chain to reach the root SV.
            // A 2D write ends at ARRAY_READ(SV, i); a 3D write ends at
            // ARRAY_READ(ARRAY_READ(SV, i), j), so we loop rather than
            // checking for a single ARRAY_READ.
            while (pool.is_function_application(base) &&
                   pool.op(base) == ExprOperator::ARRAY_READ &&
                   pool.argument_count(base) >= 1) {
                base = pool.argument(base, 0);
                modified_fluents.insert(base);
            }
        }
    }
    return modified_fluents;
}

bool AchieversAnalysis::fluent_sets_intersect(const std::unordered_set<ExprID>& set1, const std::unordered_set<ExprID>& set2) {
    for (ExprID eid : set1) {
        if (set2.contains(eid)) return true;
    }
    return false;
}

void AchieversAnalysis::initialize_persistent_solver() {
    auto start = std::chrono::high_resolution_clock::now();
    add_bounds_constraints_to_solver();
    auto end = std::chrono::high_resolution_clock::now();
    double init_time = std::chrono::duration<double>(end - start).count();

    auto& stats = Stats::instance();
    stats.set("achievers_analysis.solver_init_time_seconds", init_time);
}

void AchieversAnalysis::add_bounds_constraints_to_solver() {
    // Add bounds constraints for all state variables in both timesteps
    for (const auto& [fluent_eid, interval] : state_variable_bounds_) {
        // Add bounds for current state (timestep 0)
        z3::expr fluent_current = visitor_->convert_from_pool(fluent_eid, 0);
        if (!std::isinf(interval.lower)) {
            persistent_solver_->add(fluent_current >= variable_factory_->make_numeric_val(interval.lower));
        }
        if (!std::isinf(interval.upper)) {
            persistent_solver_->add(fluent_current <= variable_factory_->make_numeric_val(interval.upper));
        }

        // Add bounds for next state (timestep 1)
        z3::expr fluent_next = visitor_->convert_from_pool(fluent_eid, 1);
        if (!std::isinf(interval.lower)) {
            persistent_solver_->add(fluent_next >= variable_factory_->make_numeric_val(interval.lower));
        }
        if (!std::isinf(interval.upper)) {
            persistent_solver_->add(fluent_next <= variable_factory_->make_numeric_val(interval.upper));
        }
    }
}

bool AchieversAnalysis::check_action_achieves_condition_with_pushpop(const Action& action, ExprID condition_eid) {
    // Push a new solver scope
    persistent_solver_->push();

    // Step 1: Encode action effects
    bool has_effects = false;
    const ExprPool& pool = problem_->pool();

    // Multiple effects on the same array (e.g. forall-expanded writes) must be
    // chained: each write's result feeds into the next as the "current" value.
    // Adding separate arr_next == store(arr_cur, i, v) constraints for each
    // effect creates conflicting equalities that are only jointly satisfiable
    // when all writes land at the same index or the array is trivially uniform.
    // Instead we track the intermediate value after each write and emit a single
    // arr_next == final_intermediate constraint at the end.
    std::unordered_map<ExprID, z3::expr> array_intermediates;
    auto get_array_intermediate = [&](ExprID sv_id) -> z3::expr {
        auto it = array_intermediates.find(sv_id);
        if (it != array_intermediates.end()) return it->second;
        return visitor_->convert_from_pool(sv_id, 0);
    };

    for (const auto& effect_wrapper : action.effects()) {
        const EffectExpression& effect = effect_wrapper.effect_expression();

        ExprID fluent_id = effect.fluent_id();
        ExprID value_id  = effect.value_id();

        // N-D array write: ARRAY_WRITE(ARRAY_READ(...ARRAY_READ(SV,i)...,j),k) := val
        // Peel the ARRAY_READ chain to find the root SV and all indices (outermost-first),
        // then build a nested store chain. Works for 1D, 2D, 3D, and any higher dimension.
        if (pool.is_function_application(fluent_id) &&
            pool.op(fluent_id) == ExprOperator::ARRAY_WRITE &&
            pool.argument_count(fluent_id) >= 2) {

            // Collect indices innermost-first by walking the ARRAY_READ chain.
            std::vector<ExprID> rev_indices;
            rev_indices.push_back(pool.argument(fluent_id, 1));  // outermost index
            ExprID cur = pool.argument(fluent_id, 0);
            while (pool.is_function_application(cur) &&
                   pool.op(cur) == ExprOperator::ARRAY_READ &&
                   pool.argument_count(cur) >= 2) {
                rev_indices.push_back(pool.argument(cur, 1));
                cur = pool.argument(cur, 0);
            }
            ExprID base_sv = cur;  // root STATE_VARIABLE

            // Reverse to outermost-first order.
            std::vector<z3::expr> z3_indices;
            z3_indices.reserve(rev_indices.size());
            for (auto it = rev_indices.rbegin(); it != rev_indices.rend(); ++it)
                z3_indices.push_back(visitor_->convert_from_pool(*it, 0));
            z3::expr arr_cur = get_array_intermediate(base_sv);

            // [XTS] Array-of-sets cell mutation: bins[src] := SetRemove(item, bins[src]).
            // SET_ADD/SET_REMOVE aren't standalone-convertible expressions (they only
            // mean something as an effect delta) — same reasoning as the plain-set-fluent
            // branch below, just needing the cell's *current* set value first since the
            // target here is a compound array-write, not a bare fluent.
            z3::expr val = [&]() -> z3::expr {
                if (pool.is_function_application(value_id) && pool.argument_count(value_id) >= 1 &&
                    (pool.op(value_id) == ExprOperator::SET_ADD ||
                     pool.op(value_id) == ExprOperator::SET_REMOVE)) {
                    z3::expr current_set = arr_cur;
                    for (const z3::expr& idx : z3_indices) current_set = z3::select(current_set, idx);
                    z3::expr elem = visitor_->convert_from_pool(pool.argument(value_id, 0), 0);
                    bool adding = (pool.op(value_id) == ExprOperator::SET_ADD);
                    return z3::store(current_set, elem, ctx_->bool_val(adding));
                }
                return visitor_->convert_from_pool(value_id, 0);
            }();

            // Build nested store: store(arr, i, store(select(arr,i), j, ... val ...))
            std::function<z3::expr(const z3::expr&, size_t)> build_store =
                [&](const z3::expr& arr, size_t from) -> z3::expr {
                if (from == z3_indices.size() - 1)
                    return z3::store(arr, z3_indices[from], val);
                return z3::store(arr, z3_indices[from],
                                 build_store(z3::select(arr, z3_indices[from]), from + 1));
            };
            z3::expr new_arr = build_store(arr_cur, 0);

            if (effect.is_conditional()) {
                z3::expr cond = visitor_->convert_from_pool(effect.condition_id(), 0);
                new_arr = z3::ite(cond, new_arr, get_array_intermediate(base_sv));
            }
            array_intermediates.insert_or_assign(base_sv, new_arr);
            has_effects = true;
            continue;
        }

        // Set write: value_id is SET_ADD(elem) or SET_REMOVE(elem) →
        //   set_1 = store(set_0, elem, true/false)
        if (pool.is_function_application(value_id) &&
            pool.argument_count(value_id) >= 1) {
            ExprOperator val_op = pool.op(value_id);
            if (val_op == ExprOperator::SET_ADD || val_op == ExprOperator::SET_REMOVE) {
                z3::expr set_current = visitor_->convert_from_pool(fluent_id, 0);
                z3::expr set_next    = visitor_->convert_from_pool(fluent_id, 1);
                z3::expr elem = visitor_->convert_from_pool(pool.argument(value_id, 0), 0);
                bool adding = (val_op == ExprOperator::SET_ADD);
                z3::expr effect_constraint = (set_next == z3::store(set_current, elem, ctx_->bool_val(adding)));
                if (effect.is_conditional()) {
                    z3::expr cond = visitor_->convert_from_pool(effect.condition_id(), 0);
                    effect_constraint = z3::implies(cond, effect_constraint);
                }
                persistent_solver_->add(effect_constraint);
                has_effects = true;
                continue;
            }
        }

        // Scalar / whole-array-assign path
        z3::expr fluent_current = visitor_->convert_from_pool(fluent_id, 0);
        z3::expr fluent_next = visitor_->convert_from_pool(fluent_id, 1);
        z3::expr value_z3 = visitor_->convert_from_pool(value_id, 0);

        // Handle different effect kinds
        z3::expr effect_constraint = ctx_->bool_val(true);
        switch (effect.kind()) {
            case EffectExpression::Kind::ASSIGN:
                effect_constraint = (fluent_next == value_z3);
                break;
            case EffectExpression::Kind::INCREASE:
                effect_constraint = (fluent_next == fluent_current + value_z3);
                break;
            case EffectExpression::Kind::DECREASE:
                effect_constraint = (fluent_next == fluent_current - value_z3);
                break;
        }

        // Handle conditional effects
        if (effect.is_conditional()) {
            z3::expr condition_z3 = visitor_->convert_from_pool(effect.condition_id(), 0);
            effect_constraint = z3::implies(condition_z3, effect_constraint);
        }

        persistent_solver_->add(effect_constraint);
        has_effects = true;
    }

    // Flush chained array writes: add arr_next == final_intermediate for each
    // array that received at least one write effect.
    for (auto& [sv_id, final_arr] : array_intermediates) {
        z3::expr arr_next = visitor_->convert_from_pool(sv_id, 1);
        persistent_solver_->add(arr_next == final_arr);
    }

    // If action has no effects, it cannot achieve anything
    if (!has_effects) {
        persistent_solver_->pop();
        return false;
    }

    // Step 2: Encode action precondition — the action can only fire when
    // its precondition holds.  Without this, we over-approximate: e.g.
    // fly(city2, city2) would be reported as an achiever of "at city2"
    // even though it can only fire when the plane is already at city2.
    if (action.has_precondition()) {
        z3::expr precond_z3 = visitor_->convert_from_pool(action.precondition_id(), 0);
        persistent_solver_->add(precond_z3);
    }

    // Step 3: Add frame axioms for fluents not modified by this action
    auto modified_fluents = get_action_modified_fluents(action);
    auto condition_fluents = collect_fluents_in_expression(condition_eid);

    for (ExprID fluent_eid : condition_fluents) {
        if (!modified_fluents.contains(fluent_eid)) {
            z3::expr fluent_current = visitor_->convert_from_pool(fluent_eid, 0);
            z3::expr fluent_next = visitor_->convert_from_pool(fluent_eid, 1);
            persistent_solver_->add(fluent_current == fluent_next);
        }
    }

    // Step 4: Encode the transition requirement: condition becomes true
    z3::expr condition_current = visitor_->convert_from_pool(condition_eid, 0);
    z3::expr condition_next = visitor_->convert_from_pool(condition_eid, 1);

    // The key requirement: condition is false at current state, true at next state
    persistent_solver_->add(!condition_current);  // condition is currently false
    persistent_solver_->add(condition_next);       // condition becomes true after action

    // Step 5: Check if there exists such a transition
    ++z3_query_count_;
    z3::check_result result = persistent_solver_->check();

    // Pop the solver scope to remove all constraints added for this query
    persistent_solver_->pop();

    return result == z3::sat;
}

std::unordered_set<ExprID> AchieversAnalysis::collect_query_relevant_fluents(
        const Action& action, ExprID condition_eid) {
    std::unordered_set<ExprID> relevant;

    // Fluents in the condition expression
    auto cond_fluents = collect_fluents_in_expression(condition_eid);
    relevant.insert(cond_fluents.begin(), cond_fluents.end());

    // Fluents in action effects: assigned fluent + value expression + conditional effect condition
    for (const auto& effect_wrapper : action.effects()) {
        const EffectExpression& effect = effect_wrapper.effect_expression();
        if (effect.fluent_id().valid()) {
            relevant.insert(effect.fluent_id());
        }
        auto val_fluents = collect_fluents_in_expression(effect.value_id());
        relevant.insert(val_fluents.begin(), val_fluents.end());
        if (effect.is_conditional()) {
            auto ce_fluents = collect_fluents_in_expression(effect.condition_id());
            relevant.insert(ce_fluents.begin(), ce_fluents.end());
        }
    }

    // Fluents in action precondition
    if (action.has_precondition()) {
        auto prec_fluents = collect_fluents_in_expression(action.precondition_id());
        relevant.insert(prec_fluents.begin(), prec_fluents.end());
    }

    return relevant;
}

void AchieversAnalysis::update(const Problem& new_problem, RPGData new_rpg_data) {
    auto start_total = std::chrono::high_resolution_clock::now();

    // 1. Compute which fluent bounds changed
    changed_bound_fluents_.clear();
    for (const auto& [eid, new_iv] : new_rpg_data.state_variable_bounds) {
        auto it = state_variable_bounds_.find(eid);
        if (it == state_variable_bounds_.end() || it->second != new_iv) {
            changed_bound_fluents_.insert(eid);
        }
    }
    for (const auto& [eid, old_iv] : state_variable_bounds_) {
        if (!new_rpg_data.state_variable_bounds.count(eid)) {
            changed_bound_fluents_.insert(eid);
        }
    }

    Logger::instance().component(VerbosityLevel::INFO, "Achievers", {
        {"mode", "incremental"},
        {"changed_bounds", std::to_string(changed_bound_fluents_.size())},
        {"cached_achiever_conditions", std::to_string(achiever_relevant_fluents_.size())}
    });

    // 2. Update stored state
    problem_ = &new_problem;
    state_variable_bounds_ = std::move(new_rpg_data.state_variable_bounds);
    action_id_to_first_layer_ = std::move(new_rpg_data.action_first_layers);
    arpg_num_layers_ = new_rpg_data.layer_count;

    // 3. Recreate SMT infrastructure (new bounds → new solver)
    //    Destroy in reverse dependency order to avoid use-after-free:
    //    solver depends on context, visitor depends on context + factory.
    persistent_solver_.reset();
    visitor_.reset();
    variable_factory_.reset();
    ctx_.reset();
    ctx_ = std::make_unique<z3::context>();
    variable_factory_ = std::make_unique<Z3VariableFactory>(*ctx_);
    variable_factory_->set_problem(problem_);
    visitor_ = std::make_unique<GroundedEncodingVisitor>(*ctx_, problem_, variable_factory_.get());
    persistent_solver_ = std::make_unique<z3::solver>(*ctx_);
    initialize_persistent_solver();

    // 4. Re-analyze: analyze() rebuilds precondition/condition maps (cheap),
    //    then analyze_semantic_achievers() takes the incremental path because
    //    achiever_relevant_fluents_ is populated.
    auto start_analysis = std::chrono::high_resolution_clock::now();
    analyze(new_problem);
    auto end_analysis = std::chrono::high_resolution_clock::now();

    double analysis_time = std::chrono::duration<double>(end_analysis - start_analysis).count();

    // 5. Clear transient state
    changed_bound_fluents_.clear();

    auto end_total = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double>(end_total - start_total).count();

    auto& stats = Stats::instance();
    stats.set("achievers_analysis.update_time_seconds", total_time);
    stats.set("achievers_analysis.semantic_analysis_time_seconds", analysis_time);

    Logger::instance().component(VerbosityLevel::INFO, "Achievers", {
        {"update_semantic", std::to_string(analysis_time) + "s"},
        {"update_total", std::to_string(total_time) + "s"},
        {"mem", std::to_string(static_cast<int>(MemoryTracker::instance().get_current_memory_mb())) + "MB"}
    });
}

void AchieversAnalysis::print_analysis() const {
    std::cout << "=== AchieversAnalysis ===" << std::endl;

    // Print precondition analysis
    std::cout << "\n1. PRECONDITION ANALYSIS" << std::endl;
    std::cout << "Mapping preconditions to actions that require them:" << std::endl;
    std::cout << std::endl;

    if (precondition_to_actions_.empty()) {
        std::cout << "No preconditions found." << std::endl;
    } else {
        for (const auto& [precondition_eid, actions] : precondition_to_actions_) {
            std::cout << "Precondition: " << problem_->pool().to_string(precondition_eid) << std::endl;
            std::cout << "  Appears in actions:" << std::endl;

            for (const Action& action : actions) {
                std::cout << "    - " << action.name() << std::endl;
            }
            std::cout << std::endl;
        }

        std::cout << "Total unique precondition literals: " << precondition_to_actions_.size() << std::endl;
    }

    // Print achievements analysis
    std::cout << "\n2. ACHIEVED CONDITIONS ANALYSIS (Semantic SMT-based: Boolean + Numeric)" << std::endl;
    std::cout << "Mapping actions to conditions they achieve:" << std::endl;
    std::cout << std::endl;

    if (action_to_achieved_conditions_.empty()) {
        std::cout << "No semantically achieved conditions found." << std::endl;
    } else {
        for (const auto& [action, achieved_conditions] : action_to_achieved_conditions_) {
            std::cout << "Action: " << action.name() << std::endl;
            std::cout << "  Can achieve conditions:" << std::endl;

            for (ExprID eid : achieved_conditions) {
                std::cout << "    - " << problem_->pool().to_string(eid) << std::endl;
            }
            std::cout << std::endl;
        }

        std::cout << "Total actions with achieved conditions: " << action_to_achieved_conditions_.size() << std::endl;
    }

    // Print achievers analysis (reverse mapping)
    std::cout << "\n3. ACHIEVERS ANALYSIS (Semantic SMT-based: Boolean + Numeric)" << std::endl;
    std::cout << "Mapping conditions to actions that can achieve them:" << std::endl;
    std::cout << std::endl;

    if (condition_to_achievers_.empty()) {
        std::cout << "No conditions found." << std::endl;
    } else {
        // Separate conditions into achievable and unachievable
        std::vector<std::pair<ExprID, std::unordered_set<Action>>> achievable_conditions;
        std::vector<ExprID> unachievable_conditions;

        for (const auto& [condition_eid, achievers] : condition_to_achievers_) {
            if (achievers.empty()) {
                unachievable_conditions.push_back(condition_eid);
            } else {
                achievable_conditions.push_back({condition_eid, achievers});
            }
        }

        // Print achievable conditions
        if (!achievable_conditions.empty()) {
            std::cout << "CONDITIONS WITH ACHIEVERS:" << std::endl;
            for (const auto& [condition_eid, achievers] : achievable_conditions) {
                std::cout << "Condition: " << problem_->pool().to_string(condition_eid) << std::endl;
                std::cout << "  Can be achieved by actions:" << std::endl;

                for (const Action& action : achievers) {
                    std::cout << "    - " << action.name() << std::endl;
                }
                std::cout << std::endl;
            }
            std::cout << "Total achievable conditions: " << achievable_conditions.size() << std::endl;
            std::cout << std::endl;
        }

        // Print unachievable conditions
        if (!unachievable_conditions.empty()) {
            std::cout << " CONDITIONS WITHOUT ACHIEVERS (POTENTIAL ISSUES):" << std::endl;
            for (ExprID eid : unachievable_conditions) {
                std::cout << " " << problem_->pool().to_string(eid) << std::endl;
            }
            std::cout << std::endl;
            std::cout << " Total unachievable conditions: " << unachievable_conditions.size() << std::endl;
        } else {
            std::cout << " All conditions have achievers! :)" << std::endl;
        }
    }

    // Print preconditions analysis
    std::cout << "\n4. ACTION PRECONDITIONS ANALYSIS" << std::endl;
    std::cout << "Mapping actions to preconditions they require:" << std::endl;
    std::cout << std::endl;

    if (action_to_preconditions_.empty()) {
        std::cout << "No action preconditions found." << std::endl;
    } else {
        for (const auto& [action, preconditions] : action_to_preconditions_) {
            std::cout << "Action: " << action.name() << std::endl;
            std::cout << "  Requires preconditions:" << std::endl;

            for (ExprID eid : preconditions) {
                std::cout << "    - " << problem_->pool().to_string(eid) << std::endl;
            }
            std::cout << std::endl;
        }

        std::cout << "Total actions with preconditions: " << action_to_preconditions_.size() << std::endl;
    }

    // Print goal achiever analysis
    std::cout << "\n5. GOAL ACHIEVER ANALYSIS" << std::endl;
    std::cout << "Mapping goal conditions to actions that can achieve them:" << std::endl;
    std::cout << std::endl;

    if (goal_conditions_.empty()) {
        std::cout << "No goal conditions found." << std::endl;
    } else {
        for (ExprID goal_eid : goal_conditions_) {
            std::cout << "Goal condition: " << problem_->pool().to_string(goal_eid) << std::endl;

            auto achievers_it = condition_to_achievers_.find(goal_eid);
            if (achievers_it != condition_to_achievers_.end() && !achievers_it->second.empty()) {
                std::cout << "  Can be achieved by actions:" << std::endl;
                for (const Action& achiever : achievers_it->second) {
                    std::cout << "    - " << achiever.name() << std::endl;
                }
            } else {
                std::cout << "  No actions found that can achieve this goal condition!" << std::endl;
            }
            std::cout << std::endl;
        }

        std::cout << "Total goal conditions: " << goal_conditions_.size() << std::endl;

        // Count how many goal conditions have achievers
        size_t achievable_goals = 0;
        for (ExprID goal_eid : goal_conditions_) {
            if (condition_to_achievers_.contains(goal_eid) &&
                !condition_to_achievers_.at(goal_eid).empty()) {
                achievable_goals++;
            }
        }
        std::cout << "Goal conditions with achievers: " << achievable_goals << std::endl;
        std::cout << "Goal conditions without achievers: " << (goal_conditions_.size() - achievable_goals) << std::endl;
    }

    std::cout << "=========================" << std::endl;
}

int AchieversAnalysis::get_action_first_layer(int action_id) const {
    auto it = action_id_to_first_layer_.find(action_id);
    return (it != action_id_to_first_layer_.end()) ? it->second : 0;
}

std::unordered_set<size_t> AchieversAnalysis::compute_goal_relevant_action_indices(
        const Problem& problem, bool include_cost_fluents) const {
    // Build action_id → index map
    std::unordered_map<int, size_t> action_id_to_index;
    for (size_t i = 0; i < problem.actions().size(); ++i) {
        action_id_to_index[problem.actions()[i].id()] = i;
    }

    // Build action_id → Action pointer map for achiever lookups
    std::unordered_map<int, const Action*> action_id_to_ptr;
    for (const Action& a : problem.actions()) {
        action_id_to_ptr[a.id()] = &a;
    }

    // Phase 1: BFS from goal conditions through achiever chains
    std::unordered_set<const Action*> relevant_actions;
    std::vector<ExprID> frontier;
    std::unordered_set<ExprID> visited_conditions;

    for (ExprID g : goal_conditions_) {
        frontier.push_back(g);
        visited_conditions.insert(g);
    }

    while (!frontier.empty()) {
        std::vector<ExprID> next_frontier;
        for (ExprID cond : frontier) {
            auto ach_it = condition_to_achievers_.find(cond);
            if (ach_it == condition_to_achievers_.end()) continue;

            for (const Action& achiever : ach_it->second) {
                auto ptr_it = action_id_to_ptr.find(achiever.id());
                if (ptr_it == action_id_to_ptr.end()) continue;

                if (relevant_actions.insert(ptr_it->second).second) {
                    // New relevant action — add its preconditions to frontier
                    auto prec_it = action_to_preconditions_.find(achiever);
                    if (prec_it != action_to_preconditions_.end()) {
                        for (ExprID prec : prec_it->second) {
                            if (visited_conditions.insert(prec).second) {
                                next_frontier.push_back(prec);
                            }
                        }
                    }
                    // Also chase conditional-effect guards: the effect only fires
                    // if its guard holds, so whoever achieves the guard is relevant
                    // too. Depends on the guard registration in analyze() — without
                    // it these lookups miss above and this loop is a no-op.
                    const Action* act_ptr = ptr_it->second;
                    for (const Effect& eff : act_ptr->effects()) {
                        if (!eff.is_conditional()) continue;
                        std::vector<ExprID> cond_literals;
                        extract_cnf_literals(eff.effect_expression().condition_id(), cond_literals);
                        for (ExprID clit : cond_literals) {
                            if (visited_conditions.insert(clit).second) {
                                next_frontier.push_back(clit);
                            }
                        }
                    }
                }
            }
        }
        frontier = std::move(next_frontier);
    }

    // Phase 2 (SDAC safety): for each relevant action, collect fluents from
    // its cost expression. Any action that modifies those fluents is also relevant.
    if (include_cost_fluents) {
        bool changed = true;
        while (changed) {
            changed = false;
            std::unordered_set<ExprID> cost_fluents;

            for (const Action* a : relevant_actions) {
                ExprID cost = a->cost_id();
                if (!cost.valid()) continue;

                FluentCollector collector(problem);
                collector.collect_from_id(cost);
                for (ExprID f : collector.get_fluents()) {
                    cost_fluents.insert(f);
                }
            }

            if (cost_fluents.empty()) break;

            // Find actions that modify any cost fluent
            for (const Action& action : problem.actions()) {
                auto ptr_it = action_id_to_ptr.find(action.id());
                if (ptr_it == action_id_to_ptr.end()) continue;
                if (relevant_actions.count(ptr_it->second)) continue;

                for (const auto& eff : action.effects()) {
                    ExprID fluent = eff.effect_expression().fluent_id();
                    if (fluent.valid() && cost_fluents.count(fluent)) {
                        relevant_actions.insert(ptr_it->second);
                        changed = true;

                        // Also add this action's precondition conditions to
                        // the achiever BFS (its enablers are also relevant).
                        auto prec_it = action_to_preconditions_.find(action);
                        if (prec_it != action_to_preconditions_.end()) {
                            std::vector<ExprID> cost_frontier;
                            for (ExprID prec : prec_it->second) {
                                if (visited_conditions.insert(prec).second) {
                                    cost_frontier.push_back(prec);
                                }
                            }
                            // Mini BFS for enablers of cost-relevant actions
                            while (!cost_frontier.empty()) {
                                std::vector<ExprID> next;
                                for (ExprID c : cost_frontier) {
                                    auto it = condition_to_achievers_.find(c);
                                    if (it == condition_to_achievers_.end()) continue;
                                    for (const Action& en : it->second) {
                                        auto ep = action_id_to_ptr.find(en.id());
                                        if (ep == action_id_to_ptr.end()) continue;
                                        if (relevant_actions.insert(ep->second).second) {
                                            auto pi = action_to_preconditions_.find(en);
                                            if (pi != action_to_preconditions_.end()) {
                                                for (ExprID p : pi->second) {
                                                    if (visited_conditions.insert(p).second) {
                                                        next.push_back(p);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                cost_frontier = std::move(next);
                            }
                        }
                        break;  // This action is now relevant, move to next
                    }
                }
            }
        }
    }

    // Phase 3: effect-value dependency tracking for set/array fluents.
    // The BFS above only follows precondition chains. For set/array effects whose
    // *value* reads other fluents (e.g. result = union(bucket_a, bucket_b)),
    // the actions that write those read-fluents are also relevant — without them
    // the effect value is wrong, not just blocked by a precondition.
    // Example: merge_all writes result via union(bucket_a,...); add_to_a writes
    // bucket_a but has no precondition in the frontier, so Phase 1 misses it.
    {
        bool changed = true;
        while (changed) {
            changed = false;

            // Collect all fluents read by effect *values* of currently-relevant actions.
            std::unordered_set<ExprID> effect_value_fluents;
            for (const Action* a : relevant_actions) {
                for (const Effect& eff : a->effects()) {
                    const EffectExpression& ee = eff.effect_expression();
                    if (!ee.value_id().valid()) continue;
                    FluentCollector collector(problem);
                    collector.collect_from_id(ee.value_id());
                    for (ExprID f : collector.get_fluents())
                        effect_value_fluents.insert(f);
                }
            }
            if (effect_value_fluents.empty()) break;

            // Any action that writes to one of those fluents is also relevant.
            for (const Action& action : problem.actions()) {
                auto ptr_it = action_id_to_ptr.find(action.id());
                if (ptr_it == action_id_to_ptr.end()) continue;
                if (relevant_actions.count(ptr_it->second)) continue;

                bool writes_needed = false;
                for (const Effect& eff : action.effects()) {
                    ExprID fid = eff.effect_expression().fluent_id();
                    // Peel any ARRAY_WRITE wrapper to get the root SV.
                    while (fid.valid() &&
                           problem.pool().is_function_application(fid) &&
                           problem.pool().op(fid) == ExprOperator::ARRAY_WRITE &&
                           problem.pool().argument_count(fid) >= 1) {
                        fid = problem.pool().argument(fid, 0);
                    }
                    // Also peel ARRAY_READ for 2-D+ writes.
                    while (fid.valid() &&
                           problem.pool().is_function_application(fid) &&
                           problem.pool().op(fid) == ExprOperator::ARRAY_READ &&
                           problem.pool().argument_count(fid) >= 1) {
                        fid = problem.pool().argument(fid, 0);
                    }
                    if (fid.valid() && effect_value_fluents.count(fid)) {
                        writes_needed = true;
                        break;
                    }
                }

                if (writes_needed) {
                    relevant_actions.insert(ptr_it->second);
                    changed = true;
                }
            }
        }
    }

    // Convert to index set
    std::unordered_set<size_t> relevant_indices;
    for (const Action* a : relevant_actions) {
        auto it = action_id_to_index.find(a->id());
        if (it != action_id_to_index.end()) {
            relevant_indices.insert(it->second);
        }
    }

    return relevant_indices;
}

} // namespace rantanplan
