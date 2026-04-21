#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
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

// Forward declaration of protobuf-generated Problem class (for friend access from protobuf_io)
class Problem;

namespace rantanplan {

enum class MetricKind { NONE, MINIMIZE_ACTION_COSTS, MINIMIZE_PLAN_LENGTH };

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
    // Allow protobuf_io to access private members for deserialization
    friend Problem load_problem_from_protobuf(const ::Problem& pb_problem);

public:
    // Constructors
    Problem() = default;
    Problem(const std::string& domain_name, const std::string& problem_name)
        : domain_name_(domain_name), problem_name_(problem_name) {}
    
    // Basic accessors
    const std::string& domain_name() const { return domain_name_; }
    const std::string& problem_name() const { return problem_name_; }
    
    // Object access
    const std::vector<Object>& objects() const { return objects_; }
    size_t object_count() const { return objects_.size(); }
    const Object& object(size_t index) const { return objects_[index]; }
    bool has_object(const std::string& name) const;
    const Object* find_object(const std::string& name) const;
    int find_object_index(const std::string& name) const;

    /// Returns global indices of all objects whose type is (or is a subtype of) the given type.
    /// Respects the type hierarchy via Type::is_subtype_of().
    std::vector<int> objects_of_type(const Type* type) const;
    
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
     * Returns a new Problem with replaced actions, goals, and initial state.
     * ExprPool and types vector are shared (not copied) via shared_ptr.
     * Grounded fluents are re-collected from the new data.
     * Used by static fluent simplification to rebuild the problem after
     * substituting static fluents with constants.
     */
    Problem with_simplified(std::vector<Action> new_actions,
                            std::vector<Goal> new_goals,
                            std::vector<Assignment> new_initial_state) const;

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

    // Metric access
    bool has_metric() const { return metric_kind_ != MetricKind::NONE; }
    MetricKind metric_kind() const { return metric_kind_; }
    void set_metric_kind(MetricKind kind) { metric_kind_ = kind; }
    bool has_state_dependent_costs() const;

    // Integer arithmetic mode: true iff all numeric fluents are int-typed,
    // no variable division exists, and all numeric constants are integer-valued.
    // When true, encoders use Int-sorted Z3 variables instead of Real.
    bool all_integer() const { return all_integer_; }
    void set_all_integer(bool v) { all_integer_ = v; }
    
    // Type access
    const std::vector<Type>& types() const { return *types_; }
    const Type* find_type(const std::string& name) const;
    
    // String representation
    std::string to_string() const;
    
    // Expression pool access
    /// Returns the shared expression pool (interned expressions).
    const ExprPool& pool() const { return *pool_; }
    std::shared_ptr<ExprPool> pool_ptr() const { return pool_; }

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

    /// Check if an interned expression has object type (user-defined, not bool/int/real).
    bool is_object_type(ExprID eid) const {
        const ExprNode& node = pool_->get(eid);
        if (node.type_id < 0 || static_cast<size_t>(node.type_id) >= types_->size()) return false;
        return (*types_)[node.type_id].is_object();
    }

    /// Get the Type pointer for an interned expression (O(1) via ExprPool).
    const Type* type_for_id(ExprID eid) const {
        const ExprNode& node = pool_->get(eid);
        if (node.type_id >= 0 && static_cast<size_t>(node.type_id) < types_->size()) {
            return &(*types_)[node.type_id];
        }
        return nullptr;
    }

    /// Check if an ExprID is an object constant (CONSTANT kind, string payload, object type).
    bool is_object_constant(ExprID eid) const {
        if (!eid.valid()) return false;
        const ExprNode& node = pool_->get(eid);
        return static_cast<ExprKind>(node.kind) == ExprKind::CONSTANT &&
               std::holds_alternative<std::string>(node.payload) &&
               is_object_type(eid);
    }

    /// Get the global object index for an object constant, or -1 if not an object constant.
    /// Combines is_object_constant() + find_object_index() in one call.
    int object_constant_index(ExprID eid) const {
        if (!is_object_constant(eid)) return -1;
        return find_object_index(std::get<std::string>(pool_->get(eid).payload));
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
    
    // Metric
    MetricKind metric_kind_ = MetricKind::NONE;
    bool all_integer_ = false;

    // Expression interning pool
    std::shared_ptr<ExprPool> pool_ = std::make_shared<ExprPool>();

    // Quick lookup mappings
    std::unordered_map<std::string, size_t> object_name_to_index_;
    std::unordered_map<std::string, size_t> fluent_name_to_index_;
    std::unordered_map<std::string, size_t> action_name_to_index_;
    std::unordered_map<ExprID, size_t> grounded_fluent_to_index_;
    std::unordered_map<std::string, const Type*> type_name_to_ptr_;
    
    /// Create a copy with all shared members. Used by without_actions/with_actions/etc.
    Problem clone_base() const;

    void build_object_mappings();
    void build_fluent_mappings();
    void build_action_mappings();
    void build_grounded_fluent_mappings();

    void collect_grounded_fluents();
    void resolve_type_hierarchy();
};

} // namespace rantanplan
