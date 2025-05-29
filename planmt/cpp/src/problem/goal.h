#pragma once

#include "protobuf_aliases.h"
#include "expression.h"

namespace planmt {

/**
 * @brief Simple goal representation
 * 
 * Represents a goal condition in the planning problem.
 * Much simpler than protobuf Goal - focuses on essential functionality.
 * Omits temporal planning features and goal weights.
 */
class Goal {
public:
    // Constructors
    Goal() = default;
    Goal(const Expression& goal_expr) : goal_expr_(goal_expr) {}
    Goal(const pb::Goal& pb_goal);
    
    // Accessors
    const Expression& goal_expression() const { return goal_expr_; }
    
    // Setters
    void set_goal_expression(const Expression& goal_expr) { goal_expr_ = goal_expr; }
    
    // String representation
    std::string to_string() const { return goal_expr_.to_string(); }
    
    // Convert to protobuf Goal
    pb::Goal to_protobuf() const;
    
    // Operators
    bool operator==(const Goal& other) const { return goal_expr_ == other.goal_expr_; }
    bool operator!=(const Goal& other) const { return !(*this == other); }

private:
    Expression goal_expr_;
};

} // namespace planmt
