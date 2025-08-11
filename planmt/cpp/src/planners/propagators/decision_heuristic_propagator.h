#pragma once

#include "propagator_strategy.h"
#include "../../problem/problem.h"
#include "../../encoders/grounded_encoding_visitor.h"
#include "../../config/config.h"

#include <z3++.h>
#include <optional>
#include <queue>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <random>

namespace planmt {

/**
 * @brief Subgoal in Rintanen's backward chaining algorithm
 */
struct SubGoal {
    std::string literal;     // e.g., "at_robot_loc1" 
    int timestep;           // when this needs to be true
    int priority_score;     // temporal priority (lower = higher priority)
    
    bool operator<(const SubGoal& other) const {
        return priority_score > other.priority_score; // priority queue is max-heap
    }
};

/**
 * @brief Simple action-timestep pair for candidates
 */
using ActionAtTime = std::pair<const Action*, int>;

/**
 * @brief Goal-directed decision heuristic propagator implementing Rintanen's algorithm
 * 
 * Uses backward chaining from goals to guide Z3's variable selection towards
 * actions that can support unsatisfied goal literals.
 */
class DecisionHeuristicPropagator : public z3::user_propagator_base, public PropagatorStrategy {
private:
    // Essential references for heuristic implementation
    const BaseEncoder* encoder_;
    const Problem* problem_;
    const Z3VariableFactory* variable_factory_;
    z3::context* ctx_;
    z3::solver* solver_;
    int goal_timestep_;
    
    // Core data structures for Rintanen's backward chaining algorithm
    std::priority_queue<SubGoal> subgoal_queue_;
    
    // Goal literals extracted from problem goals
    // Example: goal "(and (at robot loc1) (holding block))" -> {"at_robot_loc1", "holding_block"}
    std::set<std::string> goal_literals_;  
    
    // Which actions can make each literal true
    // Example: "at_robot_loc1" -> [move_action, teleport_action]
    std::map<std::string, std::vector<const Action*>> literal_producers_; 
    
    // What literals each action needs as preconditions
    // Example: move_action -> ["robot_free", "path_clear"]
    std::map<const Action*, std::vector<std::string>> action_preconditions_; 
    
    // Track which literals are currently supported at which timesteps
    // Example: "at_robot_loc1" -> {3, 4, 5} (supported at timesteps 3-5)
    std::map<std::string, std::set<int>> supported_at_timestep_;
    
    // Simple trail for push/pop: just track which literals became supported
    std::vector<std::pair<std::string, int>> trail_;  // (literal, timestep) pairs that became supported
    std::vector<size_t> decision_levels_;  // Boundaries for push/pop
    
public:
    DecisionHeuristicPropagator(z3::solver& solver, const Problem& problem);
    ~DecisionHeuristicPropagator() override = default;
    
    // Z3 user_propagator_base interface
    void push() override;
    void pop(unsigned num_scopes) override;
    void fixed(z3::expr const& var, z3::expr const& value) override;
    void decide(z3::expr const& val, unsigned bit, bool is_pos) override;
    z3::user_propagator_base* fresh(z3::context& ctx) override;
    
    // PropagatorStrategy interface
    void initialize(z3::solver& solver, const BaseEncoder& encoder) override;
    void register_timestep_variables(int timestep) override;
    void cleanup() override;
    std::string get_name() const override { return "heuristic"; }
    PropagatorType get_type() const override { return PropagatorType::HEURISTIC; }
    
private:
    // Core backward chaining search (implements Rintanen's Figure 1 algorithm)
    std::vector<ActionAtTime> find_supporting_actions(const std::vector<std::string>& unsupported_literals, int horizon);
    
    // Build static mappings during initialization 
    void extract_goal_literals();  // Parse goals into individual trackable literals
    void build_literal_producer_mapping();  // Map literals to actions that can make them true
    void build_action_precondition_mapping();  // Map actions to their required precondition literals
    std::vector<std::string> extract_literals_from_expression(const Expression& expr);  // Recursive goal parsing
    
    // Priority calculation for temporal ordering of subgoals
    int calculate_temporal_priority(const std::string& literal, int timestep);
    
    // Check current Z3 assignment state
    std::vector<std::string> find_unsupported_goal_literals();  // Which goal literals still need support
    bool is_literal_currently_supported(const std::string& literal);  // Has supporting action been assigned true
    bool is_action_true_at_timestep(const Action* action, int timestep);  // Query Z3 variable assignment
    
    // Candidate filtering and selection (from paper's improvements)
    std::vector<ActionAtTime> filter_candidates_by_same_goal(const std::vector<ActionAtTime>& candidates);
    ActionAtTime randomly_select_candidate(const std::vector<ActionAtTime>& candidates);
};

} // namespace planmt
