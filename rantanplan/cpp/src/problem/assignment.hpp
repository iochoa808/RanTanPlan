#pragma once

#include "protobuf_aliases.hpp"
#include "expr_pool.hpp"

namespace rantanplan {

// Forward declaration
class Problem;

/**
 * @brief Assignment
 *
 * Represents an assignment of a value to a fluent. Used to represent states, such as the initial state.
 */
class Assignment {
public:
    // Constructors
    Assignment() = default;
    Assignment(const pb::Assignment& pb_assignment, Problem* problem);
    Assignment(ExprID fluent_id, ExprID value_id, const ExprPool* pool)
        : fluent_id_(fluent_id), value_id_(value_id), pool_(pool) {}

    // Accessors
    ExprID fluent_id() const { return fluent_id_; }
    ExprID value_id() const { return value_id_; }
    void set_fluent_id(ExprID id) { fluent_id_ = id; }
    void set_value_id(ExprID id) { value_id_ = id; }

    // String representation
    std::string to_string() const;

    // Operators
    bool operator==(const Assignment& other) const {
        return fluent_id_ == other.fluent_id_ && value_id_ == other.value_id_;
    }
    bool operator!=(const Assignment& other) const { return !(*this == other); }

private:
    ExprID fluent_id_ = EXPR_NULL;
    ExprID value_id_ = EXPR_NULL;
    const ExprPool* pool_ = nullptr;
};

} // namespace rantanplan
