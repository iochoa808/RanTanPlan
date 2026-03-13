#include "effect_expression.hpp"
#include <sstream>
#include <algorithm>

namespace rantanplan {

// ============================================================================
// ValueKind classification
// ============================================================================

ValueKind classify_value_kind(ExprID value_id, const ExprPool& pool) {
    if (!value_id.valid()) return ValueKind::CONSTANT;

    const ExprNode& node = pool.get(value_id);
    ExprKind kind = static_cast<ExprKind>(node.kind);

    // Leaf nodes
    if (node.children.empty()) {
        switch (kind) {
            case ExprKind::CONSTANT:
            case ExprKind::PARAMETER:   // grounded — bound to a constant
            case ExprKind::VARIABLE:    // grounded — bound to a constant
                return ValueKind::CONSTANT;
            case ExprKind::STATE_VARIABLE:
            case ExprKind::FLUENT_SYMBOL:
                return ValueKind::LINEAR;  // x is linear (1·x)
            default:
                return ValueKind::NONLINEAR; // conservative
        }
    }

    // State variable with children (grounded fluent application like (fuel airplane1))
    if (kind == ExprKind::STATE_VARIABLE) {
        return ValueKind::LINEAR;
    }

    // Function application — classify based on operator
    ExprOperator op = static_cast<ExprOperator>(node.op);

    // Classify operand children only
    ValueKind max_child = ValueKind::CONSTANT;
    for (ExprID arg : pool.arguments(value_id)) {
        ValueKind ck = classify_value_kind(arg, pool);
        max_child = std::max(max_child, ck);
    }

    // If all operands are constant, result is constant regardless of operator
    if (max_child == ValueKind::CONSTANT) {
        return ValueKind::CONSTANT;
    }

    switch (op) {
        case ExprOperator::PLUS:
        case ExprOperator::MINUS:
            // Addition/subtraction preserve linearity: linear ± linear = linear
            return max_child;

        case ExprOperator::MULTIPLY: {
            // Multiplication: constant × X preserves X's kind.
            // linear × linear (or higher) = nonlinear.
            if (pool.argument_count(value_id) == 2) {
                ValueKind lhs = classify_value_kind(pool.argument(value_id, 0), pool);
                ValueKind rhs = classify_value_kind(pool.argument(value_id, 1), pool);
                if (lhs == ValueKind::CONSTANT) return rhs;
                if (rhs == ValueKind::CONSTANT) return lhs;
            }
            return ValueKind::NONLINEAR;
        }

        case ExprOperator::DIVIDE: {
            // Division by constant preserves kind. Division by non-constant is nonlinear.
            if (pool.argument_count(value_id) == 2) {
                ValueKind divisor = classify_value_kind(pool.argument(value_id, 1), pool);
                if (divisor == ValueKind::CONSTANT) {
                    return classify_value_kind(pool.argument(value_id, 0), pool);
                }
            }
            return ValueKind::NONLINEAR;
        }

        case ExprOperator::ABSOLUTE:
        case ExprOperator::MODULO:
        case ExprOperator::MAXIMUM:
        case ExprOperator::MINIMUM:
            // Piecewise/nonsmooth — nonlinear if any child is non-constant
            return ValueKind::NONLINEAR;

        default:
            // Unknown operator — conservative
            return ValueKind::NONLINEAR;
    }
}

// ============================================================================
// EffectExpression
// ============================================================================

std::string EffectExpression::to_string() const {
    std::ostringstream oss;

    // Add forall quantifiers if present
    if (is_quantified() && pool_) {
        oss << "(forall (";
        for (size_t i = 0; i < forall_variable_ids_.size(); ++i) {
            if (i > 0) oss << " ";
            oss << pool_->to_string(forall_variable_ids_[i]);
        }
        oss << ") ";
    }

    // Add condition if present
    if (is_conditional() && pool_) {
        oss << "(when " << pool_->to_string(condition_id_) << " ";
    }

    // Add the effect with value kind annotation
    if (pool_) {
        oss << "(" << kind_to_string() << " " << pool_->to_string(fluent_id_)
            << " " << pool_->to_string(value_id_) << ")";
    } else {
        oss << "(" << kind_to_string() << " eid:" << fluent_id_.id << " eid:" << value_id_.id << ")";
    }

    // Annotate with value kind
    oss << " [" << value_kind_to_string(value_kind_) << "]";

    // Close condition if present
    if (is_conditional()) {
        oss << ")";
    }

    // Close forall if present
    if (is_quantified()) {
        oss << ")";
    }

    return oss.str();
}

std::string EffectExpression::kind_to_string() const {
    switch (kind_) {
        case Kind::ASSIGN: return "assign";
        case Kind::INCREASE: return "increase";
        case Kind::DECREASE: return "decrease";
        default: return "unknown";
    }
}

bool EffectExpression::operator==(const EffectExpression& other) const {
    return kind_ == other.kind_ &&
           fluent_id_ == other.fluent_id_ &&
           value_id_ == other.value_id_ &&
           condition_id_ == other.condition_id_ &&
           forall_variable_ids_ == other.forall_variable_ids_;
}

} // namespace rantanplan
