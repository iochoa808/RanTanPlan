#pragma once

#include <vector>
#include "protobuf_aliases.hpp"
#include "expr_pool.hpp"

namespace rantanplan {

// Forward declaration
class Problem;

/**
 * @brief Effect expression
 *
 * Represents an effect of the form "FLUENT OP VALUE" with optional condition.
 * All expression data is stored as ExprIDs in the shared ExprPool.
 */
class EffectExpression {
public:
    // Effect kinds (simplified from protobuf)
    enum class Kind {
        ASSIGN = 0,   // fluent := value
        INCREASE = 1, // fluent += value
        DECREASE = 2  // fluent -= value
    };

    // Constructors
    EffectExpression() : kind_(Kind::ASSIGN) {}
    EffectExpression(const pb::EffectExpression& pb_effect_expr, Problem* problem);

    // Accessors
    Kind kind() const { return kind_; }
    ExprID fluent_id() const { return fluent_id_; }
    ExprID value_id() const { return value_id_; }
    ExprID condition_id() const { return condition_id_; }
    bool is_conditional() const { return has_condition_; }
    const std::vector<ExprID>& forall_variable_ids() const { return forall_variable_ids_; }

    // Setters (used during interning)
    void set_fluent_id(ExprID id) { fluent_id_ = id; }
    void set_value_id(ExprID id) { value_id_ = id; }
    void set_condition_id(ExprID id) { condition_id_ = id; }

    // Convenience methods
    bool is_assign() const { return kind_ == Kind::ASSIGN; }
    bool is_increase() const { return kind_ == Kind::INCREASE; }
    bool is_decrease() const { return kind_ == Kind::DECREASE; }
    bool is_quantified() const { return !forall_variable_ids_.empty(); }

    // String representation
    std::string to_string() const;
    std::string kind_to_string() const;

    // Operators
    bool operator==(const EffectExpression& other) const;
    bool operator!=(const EffectExpression& other) const { return !(*this == other); }

private:
    Kind kind_;
    ExprID fluent_id_ = EXPR_NULL;
    ExprID value_id_ = EXPR_NULL;
    ExprID condition_id_ = EXPR_NULL;
    bool has_condition_ = false;
    std::vector<ExprID> forall_variable_ids_;
    const ExprPool* pool_ = nullptr; // for to_string() only
};

} // namespace rantanplan
