#pragma once

#include "expr_pool.hpp"

namespace rantanplan {

/**
 * @brief Goal
 *
 * Represents a goal condition in the planning problem.
 */
class Goal {
public:
    // Constructors
    Goal() = default;
    Goal(ExprID goal_id, const ExprPool* pool)
        : goal_id_(goal_id), pool_(pool) {}

    // Accessors
    ExprID goal_id() const { return goal_id_; }
    void set_goal_id(ExprID id) { goal_id_ = id; }
    void set_pool(const ExprPool* pool) { pool_ = pool; }

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
