#pragma once

#include <vector>
#include "expr_pool.hpp"

namespace rantanplan {

/**
 * @brief Structural complexity of an effect's value expression.
 *
 * Classifies the RHS of an effect (the value_id tree) by walking the ExprPool:
 *   CONSTANT  — no state variables anywhere in the tree (e.g., 5, 3+2)
 *   LINEAR    — state variables appear only under +/- (e.g., x+5, 2*x)
 *   NONLINEAR — state variables under *, /, abs, min, max, mod, or product of two vars
 *
 * Classification is purely syntactic (ExprPool tree shape) — no problem context needed.
 */
enum class ValueKind {
    CONSTANT = 0,
    LINEAR = 1,
    NONLINEAR = 2
};

inline std::string value_kind_to_string(ValueKind vk) {
    switch (vk) {
        case ValueKind::CONSTANT:  return "constant";
        case ValueKind::LINEAR:    return "linear";
        case ValueKind::NONLINEAR: return "nonlinear";
        default:                   return "unknown";
    }
}

/// Classify the structural complexity of a value expression tree.
/// Walks the ExprPool recursively. Safe to call on any valid ExprID.
ValueKind classify_value_kind(ExprID value_id, const ExprPool& pool);

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
    EffectExpression() : kind_(Kind::ASSIGN), value_kind_(ValueKind::CONSTANT) {}

    /// Construct with forall variables (used by protobuf_io).
    EffectExpression(Kind kind, ExprID fluent_id, ExprID value_id,
                     ExprID condition_id, std::vector<ExprID> forall_variable_ids,
                     const ExprPool* pool)
        : kind_(kind)
        , value_kind_(pool ? classify_value_kind(value_id, *pool) : ValueKind::CONSTANT)
        , fluent_id_(fluent_id)
        , value_id_(value_id)
        , condition_id_(condition_id)
        , has_condition_(condition_id != EXPR_NULL && !(pool && pool->is_true_constant(condition_id)))
        , forall_variable_ids_(std::move(forall_variable_ids))
        , pool_(pool)
    {}

    /// Construct a ground effect expression directly (used by the grounder).
    EffectExpression(Kind kind, ExprID fluent_id, ExprID value_id,
                     ExprID condition_id, const ExprPool* pool)
        : kind_(kind)
        , value_kind_(pool ? classify_value_kind(value_id, *pool) : ValueKind::CONSTANT)
        , fluent_id_(fluent_id)
        , value_id_(value_id)
        , condition_id_(condition_id)
        , has_condition_(condition_id != EXPR_NULL)
        , pool_(pool)
    {}

    // Accessors
    Kind kind() const { return kind_; }
    ValueKind value_kind() const { return value_kind_; }
    ExprID fluent_id() const { return fluent_id_; }
    ExprID value_id() const { return value_id_; }
    ExprID condition_id() const { return condition_id_; }
    bool is_conditional() const { return has_condition_; }
    const std::vector<ExprID>& forall_variable_ids() const { return forall_variable_ids_; }

    // Effect kind predicates
    bool is_assign() const { return kind_ == Kind::ASSIGN; }
    bool is_increase() const { return kind_ == Kind::INCREASE; }
    bool is_decrease() const { return kind_ == Kind::DECREASE; }
    bool is_quantified() const { return !forall_variable_ids_.empty(); }

    // Value kind predicates
    bool is_constant_value() const { return value_kind_ == ValueKind::CONSTANT; }
    bool is_linear_value() const { return value_kind_ == ValueKind::LINEAR; }
    bool is_nonlinear_value() const { return value_kind_ == ValueKind::NONLINEAR; }

    // String representation
    std::string to_string() const;
    std::string kind_to_string() const;

    // Operators
    bool operator==(const EffectExpression& other) const;
    bool operator!=(const EffectExpression& other) const { return !(*this == other); }

private:
    Kind kind_;
    ValueKind value_kind_ = ValueKind::CONSTANT;
    ExprID fluent_id_ = EXPR_NULL;
    ExprID value_id_ = EXPR_NULL;
    ExprID condition_id_ = EXPR_NULL;
    bool has_condition_ = false;
    std::vector<ExprID> forall_variable_ids_;
    const ExprPool* pool_ = nullptr; // for to_string() only
};

} // namespace rantanplan
