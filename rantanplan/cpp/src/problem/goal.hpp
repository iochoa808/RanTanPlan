#pragma once

#include "protobuf_aliases.hpp"
#include "expr_pool.hpp"

namespace rantanplan {

// Forward declaration
class Problem;

/**
 * @brief Goal
 *
 * Represents a goal condition in the planning problem.
 */
class Goal {
public:
    // Constructors
    Goal() = default;
    Goal(const pb::Goal& pb_goal, Problem* problem);

    // Accessors
    ExprID goal_id() const { return goal_id_; }
    void set_goal_id(ExprID id) { goal_id_ = id; }

    // String representation
    std::string to_string() const;

    // Operators
    bool operator==(const Goal& other) const { return goal_id_ == other.goal_id_; }
    bool operator!=(const Goal& other) const { return !(*this == other); }

private:
    ExprID goal_id_ = EXPR_NULL;
    const ExprPool* pool_ = nullptr;
};

} // namespace rantanplan
