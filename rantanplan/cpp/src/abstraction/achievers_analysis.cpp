#include "achievers_analysis.hpp"
#include "../arpg/arpg.hpp"
#include "../util/memory_tracker.hpp"
#include <iostream>
#include <algorithm>

namespace rantanplan {

AchieversAnalysis::AchieversAnalysis(const Problem& problem)
    : problem_(&problem) {
    auto& stats = Stats::instance();
    auto start_total = std::chrono::high_resolution_clock::now();

    std::cout << "Starting achievers analysis..." << std::endl;

    // Initialize SMT infrastructure
    ctx_ = std::make_unique<z3::context>();
    variable_factory_ = std::make_unique<Z3VariableFactory>(*ctx_);
    visitor_ = std::make_unique<GroundedEncodingVisitor>(*ctx_, problem_, variable_factory_.get());

    // Initialize persistent solver for push/pop approach
    persistent_solver_ = std::make_unique<z3::solver>(*ctx_);

    // Time ARPG computation
    auto start_arpg = std::chrono::high_resolution_clock::now();
    double memory_before_arpg = MemoryTracker::instance().get_current_memory_mb();
    std::cout << "  Building ARPG graph..." << std::flush;

    ARPG arpg(problem);
    bool goal_reachable = arpg.construct_graph();
    // Get bounds for all state variables (already ExprID-keyed)
    state_variable_bounds_ = arpg.get_state_variable_bounds();

    // Initialize persistent solver with bounds constraints
    initialize_persistent_solver();

    auto end_arpg = std::chrono::high_resolution_clock::now();

    double arpg_time = std::chrono::duration<double>(end_arpg - start_arpg).count();
    double memory_after_arpg = MemoryTracker::instance().get_current_memory_mb();
    std::cout << " timing: " << arpg_time << "s, memory=" << memory_after_arpg << "MB"
              << " (+" << (memory_after_arpg - memory_before_arpg) << "MB)" << std::endl;

    stats.set("achievers_analysis.arpg_time_seconds", arpg_time);
    stats.set("achievers_analysis.arpg_goal_reachable", goal_reachable ? 1 : 0);
    stats.set("achievers_analysis.state_variable_bounds_count", state_variable_bounds_.size());

    // Time semantic analysis
    auto start_analysis = std::chrono::high_resolution_clock::now();
    double memory_before_analysis = MemoryTracker::instance().get_current_memory_mb();
    std::cout << "  Semantic achievers analysis..." << std::flush;

    analyze(problem);
    auto end_analysis = std::chrono::high_resolution_clock::now();

    double analysis_time = std::chrono::duration<double>(end_analysis - start_analysis).count();
    double memory_after_analysis = MemoryTracker::instance().get_current_memory_mb();
    std::cout << " timing: " << analysis_time << "s, memory=" << memory_after_analysis << "MB"
              << " (+" << (memory_after_analysis - memory_before_analysis) << "MB)" << std::endl;

    stats.set("achievers_analysis.semantic_analysis_time_seconds", analysis_time);

    auto end_total = std::chrono::high_resolution_clock::now();
    double total_time = std::chrono::duration<double>(end_total - start_total).count();
    std::cout << "*** ACHIEVERS ANALYSIS COMPLETE: total_time=" << total_time << "s, memory="
              << memory_after_analysis << "MB ***" << std::endl;
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
void AchieversAnalysis::extract_cnf_literals(ExprID eid, std::vector<ExprID>& literals) {
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

    stats.set("achievers_analysis.conditions_to_analyze", all_conditions.size());
    std::cout << "    Analyzing " << all_conditions.size() << " conditions across "
              << problem_->action_count() << " actions..." << std::endl;

    // For each condition, check which actions can achieve it semantically
    size_t condition_count = 0;
    size_t total_conditions = all_conditions.size();
    for (ExprID condition_eid : all_conditions) {
        condition_count++;
        if (condition_count % 10 == 0 || condition_count == total_conditions) {
            std::cout << "    Progress: " << condition_count << "/" << total_conditions
                      << " conditions, " << z3_query_count_ << " SMT queries" << std::endl;
        }

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

            // Use SMT to check if this action can achieve the condition
            if (check_action_achieves_condition_with_pushpop(action, condition_eid)) {
                condition_to_achievers_[condition_eid].insert(action);
                action_to_achieved_conditions_[action].insert(condition_eid);
            }
        }
    }

    // Record final statistics
    stats.set("achievers_analysis.z3_queries_count", z3_query_count_);
    stats.set("achievers_analysis.total_conditions", condition_to_achievers_.size());
    stats.set("achievers_analysis.total_achievable_conditions",
              std::count_if(condition_to_achievers_.begin(), condition_to_achievers_.end(),
                           [](const auto& pair) { return !pair.second.empty(); }));
    stats.set("achievers_analysis.total_actions_with_achievers", action_to_achieved_conditions_.size());
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
    for (const auto& effect_wrapper : action.effects()) {
        const EffectExpression& effect = effect_wrapper.effect_expression();
        ExprID eid = effect.fluent_id();
        if (eid.valid()) {
            modified_fluents.insert(eid);
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
    std::cout << "  Initializing persistent solver with bounds constraints..." << std::flush;
    auto start = std::chrono::high_resolution_clock::now();

    add_bounds_constraints_to_solver();

    auto end = std::chrono::high_resolution_clock::now();
    double init_time = std::chrono::duration<double>(end - start).count();
    std::cout << " timing: " << init_time << "s" << std::endl;

    auto& stats = Stats::instance();
    stats.set("achievers_analysis.solver_init_time_seconds", init_time);
}

void AchieversAnalysis::add_bounds_constraints_to_solver() {
    // Add bounds constraints for all state variables in both timesteps
    for (const auto& [fluent_eid, interval] : state_variable_bounds_) {
        // Add bounds for current state (timestep 0)
        z3::expr fluent_current = visitor_->convert_from_pool(fluent_eid, 0);
        if (!std::isinf(interval.lower())) {
            persistent_solver_->add(fluent_current >= ctx_->real_val(std::to_string(interval.lower()).c_str()));
        }
        if (!std::isinf(interval.upper())) {
            persistent_solver_->add(fluent_current <= ctx_->real_val(std::to_string(interval.upper()).c_str()));
        }

        // Add bounds for next state (timestep 1)
        z3::expr fluent_next = visitor_->convert_from_pool(fluent_eid, 1);
        if (!std::isinf(interval.lower())) {
            persistent_solver_->add(fluent_next >= ctx_->real_val(std::to_string(interval.lower()).c_str()));
        }
        if (!std::isinf(interval.upper())) {
            persistent_solver_->add(fluent_next <= ctx_->real_val(std::to_string(interval.upper()).c_str()));
        }
    }
}

bool AchieversAnalysis::check_action_achieves_condition_with_pushpop(const Action& action, ExprID condition_eid) {
    // Push a new solver scope
    persistent_solver_->push();

    // Step 1: Encode action effects
    bool has_effects = false;
    for (const auto& effect_wrapper : action.effects()) {
        const EffectExpression& effect = effect_wrapper.effect_expression();

        z3::expr fluent_current = visitor_->convert_from_pool(effect.fluent_id(), 0);
        z3::expr fluent_next = visitor_->convert_from_pool(effect.fluent_id(), 1);
        z3::expr value_z3 = visitor_->convert_from_pool(effect.value_id(), 0);

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

    // If action has no effects, it cannot achieve anything
    if (!has_effects) {
        persistent_solver_->pop();
        return false;
    }

    // Step 2: Add frame axioms for fluents not modified by this action
    auto modified_fluents = get_action_modified_fluents(action);
    auto condition_fluents = collect_fluents_in_expression(condition_eid);

    for (ExprID fluent_eid : condition_fluents) {
        if (!modified_fluents.contains(fluent_eid)) {
            z3::expr fluent_current = visitor_->convert_from_pool(fluent_eid, 0);
            z3::expr fluent_next = visitor_->convert_from_pool(fluent_eid, 1);
            persistent_solver_->add(fluent_current == fluent_next);
        }
    }

    // Step 3: Encode the transition requirement: condition becomes true
    z3::expr condition_current = visitor_->convert_from_pool(condition_eid, 0);
    z3::expr condition_next = visitor_->convert_from_pool(condition_eid, 1);

    // The key requirement: condition is false at current state, true at next state
    persistent_solver_->add(!condition_current);  // condition is currently false
    persistent_solver_->add(condition_next);       // condition becomes true after action

    // Step 4: Check if there exists such a transition
    ++z3_query_count_;
    z3::check_result result = persistent_solver_->check();

    // Pop the solver scope to remove all constraints added for this query
    persistent_solver_->pop();

    return result == z3::sat;
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
} // namespace rantanplan
