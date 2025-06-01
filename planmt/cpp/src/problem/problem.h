#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "protobuf_aliases.h"
#include "real.h"
#include "atom.h"
#include "parameter.h"
#include "expression.h"
#include "effect_expression.h"
#include "effect.h"
#include "object.h"
#include "fluent.h"
#include "goal.h"
#include "assignment.h"
#include "action.h"
#include "type.h"

namespace planmt {

/**
 * @brief Main problem representation class
 * 
 * This is the central class that represents a complete planning problem.
 * It contains all objects, fluents, actions, initial state, and goals.
 */
class Problem {
public:
    // Constructors
    Problem() = default;
    Problem(const std::string& domain_name, const std::string& problem_name)
        : domain_name_(domain_name), problem_name_(problem_name) {}
    Problem(const pb::Problem& pb_problem);
    
    // Basic accessors
    const std::string& domain_name() const { return domain_name_; }
    const std::string& problem_name() const { return problem_name_; }
    
    // Object access
    const std::vector<Object>& objects() const { return objects_; }
    size_t object_count() const { return objects_.size(); }
    const Object& object(size_t index) const { return objects_[index]; }
    bool has_object(const std::string& name) const;
    const Object* find_object(const std::string& name) const;
    
    // Fluent access
    const std::vector<Fluent>& fluents() const { return fluents_; }
    size_t fluent_count() const { return fluents_.size(); }
    const Fluent& fluent(size_t index) const { return fluents_[index]; }
    bool has_fluent(const std::string& name) const;
    const Fluent* find_fluent(const std::string& name) const;
    
    // Action access
    const std::vector<Action>& actions() const { return actions_; }
    size_t action_count() const { return actions_.size(); }
    const Action& action(size_t index) const { return actions_[index]; }
    bool has_action(const std::string& name) const;
    const Action* find_action(const std::string& name) const;
    
    // Initial state access
    const std::vector<Assignment>& initial_state() const { return initial_state_; }
    size_t initial_assignment_count() const { return initial_state_.size(); }
    const Assignment& initial_assignment(size_t index) const { return initial_state_[index]; }
    
    // Goal access
    const std::vector<Goal>& goals() const { return goals_; }
    size_t goal_count() const { return goals_.size(); }
    const Goal& goal(size_t index) const { return goals_[index]; }
    
    // Type access
    const std::vector<Type>& types() const { return types_; }
    const Type* find_type(const std::string& name) const;
    
    // Setters
    void set_domain_name(const std::string& domain_name) { domain_name_ = domain_name; }
    void set_problem_name(const std::string& problem_name) { problem_name_ = problem_name; }
    
    void add_object(const Object& object);
    void set_objects(const std::vector<Object>& objects);
    
    void add_fluent(const Fluent& fluent);
    void set_fluents(const std::vector<Fluent>& fluents);
    
    void add_action(const Action& action);
    void set_actions(const std::vector<Action>& actions);
    
    void add_initial_assignment(const Assignment& assignment) { initial_state_.push_back(assignment); }
    void set_initial_state(const std::vector<Assignment>& initial_state) { initial_state_ = initial_state; }
    
    void add_goal(const Goal& goal) { goals_.push_back(goal); }
    void set_goals(const std::vector<Goal>& goals) { goals_ = goals; }
    
    // Convenience methods
    void clear_all();
    bool is_empty() const;
    
    // String representation
    std::string to_string() const;
    
    // Operators
    bool operator==(const Problem& other) const;
    bool operator!=(const Problem& other) const { return !(*this == other); }

private:
    std::string domain_name_;
    std::string problem_name_;
    
    std::vector<Object> objects_;
    std::vector<Fluent> fluents_;
    std::vector<Action> actions_;
    std::vector<Assignment> initial_state_;
    std::vector<Goal> goals_;
    std::vector<Type> types_;
    
    // Quick lookup mappings
    std::unordered_map<std::string, size_t> object_name_to_index_;
    std::unordered_map<std::string, size_t> fluent_name_to_index_;
    std::unordered_map<std::string, size_t> action_name_to_index_;
    std::unordered_map<std::string, const Type*> type_name_to_ptr_;
    
    void build_object_mappings();
    void build_fluent_mappings();
    void build_action_mappings();
    void load_types(const pb::RepeatedTypeDeclaration& pb_types);
    void resolve_type_hierarchy();
    void load_objects(const pb::RepeatedObjectDeclaration& pb_objects);
    void load_fluents(const pb::RepeatedFluent& pb_fluents);
    void load_actions(const pb::RepeatedAction& pb_actions);
};

} // namespace planmt
