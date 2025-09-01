#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include "../problem/expression.h"
#include "../problem/action.h"
#include "../problem/goal.h"
#include "../problem/problem.h"
#include "../encoders/grounded_encoding_visitor.h"
#include "../encoders/z3_variable_factory.h"
#include "../problem/visitors/fluent_collector.h"
#include "../arpg/interval.h"
#include "../util/stats.h"
#include <z3++.h>
#include <memory>
#include <optional>
#include <chrono>

namespace planmt {

/**
 * @brief AchieversAnalysis
 * 
 * Maps each precondition (literal) of actions to the actions that contain it.
 * Uses semantic SMT-based checking to determine which actions can achieve conditions.
 * For each action-condition pair, checks if there exists a state where executing the
 * action transitions the condition from false to true.
 * 
 * FIXED: Added ARPG bounds analysis to prevent incorrect results from impossible variable values.
 * The SMT queries now include bounds constraints for all involved fluents, preventing scenarios
 * like negative weights that would make physically impossible transitions appear satisfiable.
 * 
 * For example, the previous issue where weight_cargo1 could be assigned negative values
 * is now prevented by ARPG bounds constraints: (assert (>= weight_cargo1_0 20.0)) etc.
 * 
 * OPTIMIZED: Uses push/pop approach with persistent solver to avoid recreating bounds constraints.
 * Instead of creating a fresh solver for each query, maintains a persistent solver with all
 * bounds constraints pre-loaded, then uses push/pop to add/remove action-specific constraints.
 * This significantly reduces SMT solver initialization overhead for large problems.
 * 
 * TODO: Replace ARPG with step-bounded reachability analysis to get tighter bounds.
 * The current ARPG provides bounds like [-inf, inf] for changeable variables (e.g., current_load)
 * because it computes asymptotic bounds over infinite time. A step-bounded analysis that
 * computes reachable values within a reasonable number of steps would provide much tighter
 * bounds like [0, 50] for current_load, eliminating most infinite bounds and improving
 * SMT constraint accuracy.
 * 
 * TODO: It is quite expensive for big problems. Consider moving the Boolean checks to syntactic checks
 * Also, can we group checks?
 */
class AchieversAnalysis {
public:
    // Constructor
    AchieversAnalysis() = delete;
    explicit AchieversAnalysis(const Problem& problem);
    
    // Build the analysis from a problem
    void analyze(const Problem& problem);
    
    // Access methods for preconditions
    std::unordered_set<Action> get_actions_requiring_precondition(const Expression& precondition) const;
    std::unordered_set<Expression> get_preconditions(const Action& action) const;
    
    // Access methods for achievers  
    std::unordered_set<Action> get_achievers(const Expression& condition) const;
    std::unordered_set<Expression> get_achieved_conditions(const Action& action) const;
    
    // Get all conditions considered
    std::unordered_set<Expression> get_all_conditions() const;
    
    // Output method
    void print_analysis() const;
    
    // Clear the analysis
    void clear();

private:
    // Map from precondition to set of actions that require it
    std::unordered_map<Expression, std::unordered_set<Action>> precondition_to_actions_;
    
    // Map from action to set of preconditions it requires
    std::unordered_map<Action, std::unordered_set<Expression>> action_to_preconditions_;
    
    // Map from condition to set of actions that can achieve it (using semantic analysis)
    std::unordered_map<Expression, std::unordered_set<Action>> condition_to_achievers_;
    
    // Map from action to set of conditions it achieves (using semantic analysis)
    std::unordered_map<Action, std::unordered_set<Expression>> action_to_achieved_conditions_;
    
    // Set of goal conditions (extracted from goal expressions)
    std::unordered_set<Expression> goal_conditions_;
    
    // Cached set of all conditions (computed once for performance)
    mutable std::unordered_set<Expression> all_conditions_cache_;
    mutable bool all_conditions_cached_ = false;
    
    // SMT solving infrastructure
    std::unique_ptr<z3::context> ctx_;
    std::unique_ptr<Z3VariableFactory> variable_factory_;
    std::unique_ptr<GroundedEncodingVisitor> visitor_;
    std::unique_ptr<z3::solver> persistent_solver_;  // Persistent solver with bounds constraints
    
    // Problem reference for SMT encoding
    const Problem* problem_;
    
    // State variable bounds from ARPG for SMT constraint generation
    std::unordered_map<Expression, Interval> state_variable_bounds_;
    
    // Statistics tracking
    mutable size_t z3_query_count_ = 0;

    // Helper methods
    void extract_cnf_literals(const Expression& expr, std::vector<Expression>& literals);
    void process_action_preconditions(const Action& action);
    void process_goal_conditions(const Goal& goal);
    void analyze_semantic_achievers();
    std::unordered_set<Expression> collect_fluents_in_expression(const Expression& expr);
    std::unordered_set<Expression> get_action_modified_fluents(const Action& action);
    bool expressions_share_fluents(const std::unordered_set<Expression>& set1, const std::unordered_set<Expression>& set2);
    std::optional<z3::expr> convert_expression_to_z3(const Expression& expr, int timestep);
    
    // SMT solver management methods for push/pop approach
    void initialize_persistent_solver();
    void add_bounds_constraints_to_solver();
    bool check_action_achieves_condition_with_pushpop(const Action& action, const Expression& condition);
};

} // namespace planmt