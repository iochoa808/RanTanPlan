#include "achievers_analysis.h"
#include "../arpg/arpg.h"
#include "../util/memory_tracker.h"
#include <iostream>
#include <algorithm>

namespace planmt {

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
    // Get bounds for all state variables using Expression keys and store them
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
    //print_analysis();
}

void AchieversAnalysis::process_action_preconditions(const Action& action) {
    if (!action.has_precondition()) return;
    
    // Extract CNF literals from the precondition
    std::vector<Expression> literals;
    extract_cnf_literals(action.precondition(), literals);
    
    // Map each literal to this action
    for (const auto& literal : literals) {
        precondition_to_actions_[literal].insert(action);
        
        // Also map this action to the required precondition
        action_to_preconditions_[action].insert(literal);

        // keep track of the split between goal conditions and precondition conditions
        pre_conditions_.insert(literal);
    }
}

void AchieversAnalysis::process_goal_conditions(const Goal& goal) {
    // Extract CNF literals from the goal expression
    std::vector<Expression> literals;
    extract_cnf_literals(goal.goal_expression(), literals);
    
    // Store goal conditions and ensure they're tracked in achiever analysis
    for (const auto& goal_condition : literals) {
        // Store this as a goal condition
        goal_conditions_.insert(goal_condition);
        
        // Ensure it's in our condition_to_achievers_ map for analysis
        // If no actions achieve this condition yet, create an empty entry
        if (!condition_to_achievers_.contains(goal_condition)) {
            condition_to_achievers_[goal_condition] = std::unordered_set<Action>();
        }
    }
}

// Extract literals from CNF expressions for precondition analysis.
// Since we use semantic SMT-based analysis for achievers, we can handle disjunctions
// naturally by preserving OR expressions as single literals. The SMT solver will
// properly evaluate disjunctive conditions without needing to split them.
void AchieversAnalysis::extract_cnf_literals(const Expression& expr, std::vector<Expression>& literals) {
    if (expr.is_and()) {
        // AND expression - extract literals from each conjunct recursively
        for (size_t i = 0; i < expr.list_size(); ++i) {
            extract_cnf_literals(expr.list_element(i), literals);
        }
    } else {
        // Base case: this is a literal (atomic or disjunctive)
        // Keep OR expressions intact as single literals since our semantic analysis
        // can handle them properly via SMT solving
        if (!expr.is_function_symbol()) {
            literals.push_back(expr);
        }
    }
}

const std::unordered_set<Action>& AchieversAnalysis::get_actions_requiring_precondition(const Expression& precondition) const {
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
    std::unordered_set<Expression> all_conditions;
    
    // Collect all preconditions (includes numeric conditions like (<= x 5))
    for (const auto& [condition, actions] : precondition_to_actions_) {
        all_conditions.insert(condition);
    }
    
    // Collect all goal conditions
    for (const auto& goal_condition : goal_conditions_) {
        all_conditions.insert(goal_condition);
    }
    
    stats.set("achievers_analysis.conditions_to_analyze", all_conditions.size());
    std::cout << "    Analyzing " << all_conditions.size() << " conditions across " 
              << problem_->action_count() << " actions..." << std::endl;
    
    // For each condition, check which actions can achieve it semantically
    size_t condition_count = 0;
    size_t total_conditions = all_conditions.size();
    for (const Expression& condition : all_conditions) {
        condition_count++;
        if (condition_count % 10 == 0 || condition_count == total_conditions) {
            std::cout << "    Progress: " << condition_count << "/" << total_conditions 
                      << " conditions, " << z3_query_count_ << " SMT queries" << std::endl;
        }
        
        auto condition_fluents = collect_fluents_in_expression(condition);
        
        // Show condition analysis with improved fluent detection
        //std::cout << "  Condition: " << condition.to_string() 
        //          << " fluents: ["; 
        
        //bool first = true;
        //for (const auto& fluent : condition_fluents) {
        //    if (!first) std::cout << ", ";
        //    std::cout << fluent.to_string();
        //    first = false;
        //}
        //std::cout << "]" << std::endl;
        
        for (const auto& action : problem_->actions()) {
            auto action_fluents = get_action_modified_fluents(action);
            
            // For non-boolean conditions, always try SMT checking since fluent collection may miss nested fluents
            bool should_test = !condition_fluents.empty() && expressions_share_fluents(condition_fluents, action_fluents);
            
            // For numeric conditions where fluent collection failed, still try if action has numeric effects
            if (!should_test && condition.is_function_application() && condition_fluents.empty()) {
                should_test = !action_fluents.empty(); // Try any action that modifies numeric fluents
            }
            
            if (!should_test) {
                continue;
            }
            
            // Use SMT to check if this action can achieve the condition
            if (check_action_achieves_condition_with_pushpop(action, condition)) {
                //std::cout << "   Action " << action.name() << " CAN achieve condition " << condition.to_string() << std::endl;
                condition_to_achievers_[condition].insert(action);
                action_to_achieved_conditions_[action].insert(condition);
                // Only show achievers for conditions that have fluents or are interesting
                //if (!condition_fluents.empty() || condition.is_function_application()) {
                //    std::cout << "   " << action.name() << " could achieve " << condition.to_string() << std::endl;
                //}
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

const std::unordered_set<Expression>& AchieversAnalysis::get_achieved_conditions(const Action& action) const {
    auto it = action_to_achieved_conditions_.find(action);
    if (it != action_to_achieved_conditions_.end()) {
        return it->second;
    }
    static const std::unordered_set<Expression> empty_set;
    return empty_set;
}

const std::unordered_set<Action>& AchieversAnalysis::get_achievers(const Expression& condition) const {
    auto it = condition_to_achievers_.find(condition);
    if (it != condition_to_achievers_.end()) {
        return it->second;
    }
    static const std::unordered_set<Action> empty_set;
    return empty_set;
}

const std::unordered_set<Expression>& AchieversAnalysis::get_preconditions(const Action& action) const {
    auto it = action_to_preconditions_.find(action);
    if (it != action_to_preconditions_.end()) {
        return it->second;
    }
    static const std::unordered_set<Expression> empty_set;
    return empty_set;
}

const std::unordered_set<Expression>& AchieversAnalysis::get_all_conditions() const {
    // Return cached result if available
    if (all_conditions_cached_) {
        return all_conditions_cache_;
    }

    // Build the cache
    all_conditions_cache_.clear();

    // Add conditions in all actions
    for (const auto& [precondition, actions] : precondition_to_actions_) {
        all_conditions_cache_.insert(precondition);
    }

    // Add all goal conditions
    for (const auto& goal_condition : goal_conditions_) {
        all_conditions_cache_.insert(goal_condition);
    }

    all_conditions_cached_ = true;
    return all_conditions_cache_;
}

const std::unordered_set<Expression>& AchieversAnalysis::get_pre_conditions() const {
    return pre_conditions_;
}

const std::unordered_set<Expression>& AchieversAnalysis::get_goal_conditions() const {
    return goal_conditions_;
}

std::unordered_set<Expression> AchieversAnalysis::collect_fluents_in_expression(const Expression& expr) {
    FluentCollector collector;
    collector.collect_from_expression(expr);
    return collector.get_fluents();
}

std::unordered_set<Expression> AchieversAnalysis::get_action_modified_fluents(const Action& action) {
    std::unordered_set<Expression> modified_fluents;
    for (const auto& effect_wrapper : action.effects()) {
        const EffectExpression& effect = effect_wrapper.effect_expression();
        modified_fluents.insert(effect.fluent());
    }
    return modified_fluents;
}

bool AchieversAnalysis::expressions_share_fluents(const std::unordered_set<Expression>& set1, const std::unordered_set<Expression>& set2) {
    for (const Expression& expr1 : set1) {
        if (set2.contains(expr1)) return true;
    }
    return false;
}

std::optional<z3::expr> AchieversAnalysis::convert_expression_to_z3(const Expression& expr, int timestep) {
    visitor_->clear();
    visitor_->set_timestep(timestep);
    accept_visitor(expr, *visitor_);
    visitor_->clear_timestep();
    return visitor_->get_result();
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
    for (const auto& [fluent, interval] : state_variable_bounds_) {
        //std::cout << "    Adding bounds for fluent: " << fluent.to_string()
        //          << " [" << interval.lower() << ", " << interval.upper() << "]" << std::endl;

        // Add bounds for current state (timestep 0)
        auto fluent_current = convert_expression_to_z3(fluent, 0);
        if (fluent_current) {
            if (!std::isinf(interval.lower())) {
                persistent_solver_->add(*fluent_current >= ctx_->real_val(std::to_string(interval.lower()).c_str()));
            }
            if (!std::isinf(interval.upper())) {
                persistent_solver_->add(*fluent_current <= ctx_->real_val(std::to_string(interval.upper()).c_str()));
            }
        }

        // Add bounds for next state (timestep 1)
        // Note: We add these bounds as they provide useful constraints for the SMT solver
        // even though action effects will further constrain the values
        auto fluent_next = convert_expression_to_z3(fluent, 1);
        if (fluent_next) {
            if (!std::isinf(interval.lower())) {
                persistent_solver_->add(*fluent_next >= ctx_->real_val(std::to_string(interval.lower()).c_str()));
            }
            if (!std::isinf(interval.upper())) {
                persistent_solver_->add(*fluent_next <= ctx_->real_val(std::to_string(interval.upper()).c_str()));
            }
        }
    }
}

bool AchieversAnalysis::check_action_achieves_condition_with_pushpop(const Action& action, const Expression& condition) {
    // Push a new solver scope
    persistent_solver_->push();
    
    // Step 1: Encode action effects
    bool has_effects = false;
    for (const auto& effect_wrapper : action.effects()) {
        const EffectExpression& effect = effect_wrapper.effect_expression();
        
        auto fluent_current = convert_expression_to_z3(effect.fluent(), 0);
        auto fluent_next = convert_expression_to_z3(effect.fluent(), 1);
        auto value_z3 = convert_expression_to_z3(effect.value(), 0);
        
        // Handle different effect kinds
        z3::expr effect_constraint = ctx_->bool_val(true);
        switch (effect.kind()) {
            case EffectExpression::Kind::ASSIGN:
                effect_constraint = (*fluent_next == *value_z3);
                break;
            case EffectExpression::Kind::INCREASE:
                effect_constraint = (*fluent_next == *fluent_current + *value_z3);
                break;
            case EffectExpression::Kind::DECREASE:
                effect_constraint = (*fluent_next == *fluent_current - *value_z3);
                break;
        }
        
        // Handle conditional effects
        if (effect.is_conditional()) {
            auto condition_z3 = convert_expression_to_z3(effect.condition(), 0);
            effect_constraint = z3::implies(*condition_z3, effect_constraint);
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
    auto condition_fluents = collect_fluents_in_expression(condition);
    
    for (const Expression& fluent : condition_fluents) {
        if (!modified_fluents.contains(fluent)) {
            auto fluent_current = convert_expression_to_z3(fluent, 0);
            auto fluent_next = convert_expression_to_z3(fluent, 1);
            if (fluent_current && fluent_next) {
                persistent_solver_->add(*fluent_current == *fluent_next);
            }
        }
    }
    
    // Step 3: Encode the transition requirement: condition becomes true
    auto condition_current = convert_expression_to_z3(condition, 0);
    auto condition_next = convert_expression_to_z3(condition, 1);
    
    // The key requirement: condition is false at current state, true at next state
    persistent_solver_->add(!(*condition_current));  // condition is currently false
    persistent_solver_->add(*condition_next);        // condition becomes true after action
    
    // Step 4: Check if there exists such a transition
    ++z3_query_count_;
    // Print all the constraints asserted to the solver for debugging
    //std::cout << "   === SMT Constraints for Action " << action.name() << " achieving " << condition.to_string() << " ===" << std::endl;
    //z3::expr_vector assertions = persistent_solver_->assertions();
    //for (unsigned i = 0; i < assertions.size(); ++i) {
    //    std::cout << "   Constraint[" << i << "]: " << assertions[i].to_string() << std::endl;
    //}
    //std::cout << "   === End of Constraints ===" << std::endl;
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
        for (const auto& [precondition, actions] : precondition_to_actions_) {
            std::cout << "Precondition: " << precondition.to_string() << std::endl;
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
            
            for (const Expression& achieved_condition : achieved_conditions) {
                std::cout << "    - " << achieved_condition.to_string() << std::endl;
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
        std::vector<std::pair<Expression, std::unordered_set<Action>>> achievable_conditions;
        std::vector<Expression> unachievable_conditions;
        
        for (const auto& [condition, achievers] : condition_to_achievers_) {
            if (achievers.empty()) {
                unachievable_conditions.push_back(condition);
            } else {
                achievable_conditions.push_back({condition, achievers});
            }
        }
        
        // Print achievable conditions
        if (!achievable_conditions.empty()) {
            std::cout << "CONDITIONS WITH ACHIEVERS:" << std::endl;
            for (const auto& [condition, achievers] : achievable_conditions) {
                std::cout << "Condition: " << condition.to_string() << std::endl;
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
            for (const Expression& condition : unachievable_conditions) {
                std::cout << " " << condition.to_string() << std::endl;
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
            
            for (const Expression& precondition : preconditions) {
                std::cout << "    - " << precondition.to_string() << std::endl;
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
        for (const Expression& goal_condition : goal_conditions_) {
            std::cout << "Goal condition: " << goal_condition.to_string() << std::endl;
            
            auto achievers_it = condition_to_achievers_.find(goal_condition);
            if (achievers_it != condition_to_achievers_.end() && !achievers_it->second.empty()) {
                std::cout << "  Can be achieved by actions:" << std::endl;
                for (const Action& achiever : achievers_it->second) {
                    std::cout << "    - " << achiever.name() << std::endl;
                }
            } else {
                std::cout << "️ No actions found that can achieve this goal condition!" << std::endl;
            }
            std::cout << std::endl;
        }
        
        std::cout << "Total goal conditions: " << goal_conditions_.size() << std::endl;
        
        // Count how many goal conditions have achievers
        size_t achievable_goals = 0;
        for (const Expression& goal_condition : goal_conditions_) {
            if (condition_to_achievers_.contains(goal_condition) && 
                !condition_to_achievers_.at(goal_condition).empty()) {
                achievable_goals++;
            }
        }
        std::cout << "Goal conditions with achievers: " << achievable_goals << std::endl;
        std::cout << "Goal conditions without achievers: " << (goal_conditions_.size() - achievable_goals) << std::endl;
    }
    
    std::cout << "=========================" << std::endl;
}
} // namespace planmt