#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "protobuf_aliases.hpp"
#include "real.hpp"
#include "atom.hpp"
#include "parameter.hpp"
#include "effect_expression.hpp"
#include "effect.hpp"
#include "object.hpp"
#include "fluent.hpp"
#include "goal.hpp"
#include "assignment.hpp"
#include "action.hpp"
#include "type.hpp"
#include "expr_pool.hpp"

namespace rantanplan {

/**
 * @brief Main problem representation class
 * 
 * This is the central class that represents a complete planning problem.
 * It contains all objects, fluents, actions, initial state, and goals.
 * 
 * Note that this class is the only one that has qualified objects. That is,
 * Fluent objects for example represent the schema of a fluent, not a grounded
 * instance. The grounded instances are represented by ExprID handles into ExprPool.
 * ExprPool provides query methods to check the type of an expression (ExprKind),
 * such as whether it is a fluent application, function application, or an atom.
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
    const Fluent& fluent_by_id(int id) const { return fluents_[id]; }
    bool has_fluent(const std::string& name) const;
    const Fluent* find_fluent(const std::string& name) const;

    // Grounded fluent access
    // NOTE: Grounded fluents are stored with IDs matching their position in the grounded_fluents_ vector.
    // This provides a stable ID system where grounded_fluent_id == index in grounded_fluents() vector.
    const std::vector<ExprID>& grounded_fluents() const { return grounded_fluents_; }
    size_t grounded_fluent_count() const { return grounded_fluents_.size(); }
    ExprID grounded_fluent(int id) const { return grounded_fluents_[id]; }

    // Grounded fluent lookup
    /**
     * Find the index of a grounded fluent by ExprID using O(1) hash map lookup.
     * @return fluent index (0-based), or -1 if not found
     */
    int find_grounded_fluent_index(ExprID eid) const;

    // Action access
    const std::vector<Action>& actions() const { return actions_; }
    size_t action_count() const { return actions_.size(); }
    const Action& action(size_t index) const { return actions_[index]; }
    bool has_action(const std::string& name) const;
    const Action* find_action(const std::string& name) const;
    
    /**
     * Returns a new Problem with the specified actions removed.
     * Action IDs in the new Problem are contiguous [0..N-1].
     * ExprPool and types vector are shared (not copied) via shared_ptr.
     * @param removed_indices Indices of actions to remove
     * @return New Problem without the specified actions
     */
    Problem without_actions(const std::vector<size_t>& removed_indices) const;

    /**
     * Returns a new Problem with the given actions replacing the current ones.
     * Action IDs are re-assigned contiguously [0..N-1].
     * ExprPool and types vector are shared (not copied) via shared_ptr.
     * All other data (objects, fluents, initial state, goals) is copied.
     * Grounded fluents are re-collected from the new action set.
     * @param new_actions The complete set of (ground) actions
     * @return New Problem with the given actions
     */
    Problem with_actions(std::vector<Action> new_actions) const;

    /**
     * Returns a new Problem with extra assignments appended to the initial state.
     * ExprPool and types vector are shared (not copied) via shared_ptr.
     * All other data (objects, fluents, actions, goals, grounded fluents) is copied.
     * @param extra_assignments Additional assignments to append
     * @return New Problem with the extended initial state
     */
    Problem with_additional_initial_state(const std::vector<Assignment>& extra_assignments) const;

    // Initial state access
    const std::vector<Assignment>& initial_state() const { return initial_state_; }
    size_t initial_assignment_count() const { return initial_state_.size(); }
    const Assignment& initial_assignment(size_t index) const { return initial_state_[index]; }
    
    // Goal access
    const std::vector<Goal>& goals() const { return goals_; }
    size_t goal_count() const { return goals_.size(); }
    const Goal& goal(size_t index) const { return goals_[index]; }
    
    // Type access
    const std::vector<Type>& types() const { return *types_; }
    const Type* find_type(const std::string& name) const;
    
    // String representation
    std::string to_string() const;
    
    // Expression pool access
    /// Returns the shared expression pool (interned expressions).
    const ExprPool& pool() const { return *pool_; }
    std::shared_ptr<ExprPool> pool_ptr() const { return pool_; }

    /// Intern a protobuf Expression directly into the pool (no Expression intermediary).
    ExprID intern_from_protobuf(const pb::Expression& pb_expr);

    /// Check if an interned expression has boolean type (O(1) via ExprPool).
    bool is_bool_type(ExprID eid) const {
        const ExprNode& node = pool_->get(eid);
        return node.type_id >= 0 &&
               static_cast<size_t>(node.type_id) < types_->size() &&
               (*types_)[node.type_id].is_bool();
    }

    /// Check if an interned expression has numeric type (int or real).
    bool is_numeric_type(ExprID eid) const {
        const ExprNode& node = pool_->get(eid);
        if (node.type_id < 0 || static_cast<size_t>(node.type_id) >= types_->size()) return false;
        return (*types_)[node.type_id].is_int() || (*types_)[node.type_id].is_real();
    }

    /// Get the Type pointer for an interned expression (O(1) via ExprPool).
    const Type* type_for_id(ExprID eid) const {
        const ExprNode& node = pool_->get(eid);
        if (node.type_id >= 0 && static_cast<size_t>(node.type_id) < types_->size()) {
            return &(*types_)[node.type_id];
        }
        return nullptr;
    }

private:
    std::string domain_name_;
    std::string problem_name_;
    
    std::vector<Object> objects_;
    std::vector<Fluent> fluents_;
    std::vector<Action> actions_;
    std::vector<ExprID> grounded_fluents_;
    std::vector<Assignment> initial_state_;
    std::vector<Goal> goals_;
    std::shared_ptr<std::vector<Type>> types_ = std::make_shared<std::vector<Type>>();
    
    // Expression interning pool
    std::shared_ptr<ExprPool> pool_ = std::make_shared<ExprPool>();

    // Quick lookup mappings
    std::unordered_map<std::string, size_t> object_name_to_index_;
    std::unordered_map<std::string, size_t> fluent_name_to_index_;
    std::unordered_map<std::string, size_t> action_name_to_index_;
    std::unordered_map<ExprID, size_t> grounded_fluent_to_index_;
    std::unordered_map<std::string, const Type*> type_name_to_ptr_;
    
    void build_object_mappings();
    void build_fluent_mappings();
    void build_action_mappings();
    void build_grounded_fluent_mappings();

    void collect_grounded_fluents();
    void load_types(const pb::RepeatedTypeDeclaration& pb_types);
    void resolve_type_hierarchy();
    void load_objects(const pb::RepeatedObjectDeclaration& pb_objects);
    void load_fluents(const pb::RepeatedFluent& pb_fluents);
    void load_actions(const pb::RepeatedAction& pb_actions);
};

} // namespace rantanplan
