#include "effect_expression.hpp"
#include "problem.hpp"
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
    // In ExprPool, FUNCTION_APPLICATION nodes have children[0] = operator symbol.
    // Actual operands start at index 1, so a binary op has 3 children total.
    ExprOperator op = static_cast<ExprOperator>(node.op);

    // Classify operand children only (skip children[0] which is the operator symbol)
    ValueKind max_child = ValueKind::CONSTANT;
    for (size_t i = 1; i < node.children.size(); ++i) {
        ValueKind ck = classify_value_kind(node.children[i], pool);
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
            // Binary multiply has 3 children: [0]=op, [1]=lhs, [2]=rhs
            if (node.children.size() == 3) {
                ValueKind lhs = classify_value_kind(node.children[1], pool);
                ValueKind rhs = classify_value_kind(node.children[2], pool);
                if (lhs == ValueKind::CONSTANT) return rhs;
                if (rhs == ValueKind::CONSTANT) return lhs;
            }
            return ValueKind::NONLINEAR;
        }

        case ExprOperator::DIVIDE: {
            // Division by constant preserves kind. Division by non-constant is nonlinear.
            // Binary divide has 3 children: [0]=op, [1]=numerator, [2]=divisor
            if (node.children.size() == 3) {
                ValueKind divisor = classify_value_kind(node.children[2], pool);
                if (divisor == ValueKind::CONSTANT) {
                    return classify_value_kind(node.children[1], pool);
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

EffectExpression::EffectExpression(const pb::EffectExpression& pb_effect_expr, Problem* problem)
    : kind_(static_cast<Kind>(pb_effect_expr.kind())) {

    pool_ = &problem->pool();

    fluent_id_ = problem->intern_from_protobuf(pb_effect_expr.fluent());
    value_id_ = problem->intern_from_protobuf(pb_effect_expr.value());

    // Classify the value expression structure
    value_kind_ = classify_value_kind(value_id_, *pool_);

    if (pb_effect_expr.has_condition()) {
        condition_id_ = problem->intern_from_protobuf(pb_effect_expr.condition());
        // Check if condition is a trivial "true" constant
        has_condition_ = condition_id_.valid() && !pool_->is_true_constant(condition_id_);
    }

    for (const auto& var : pb_effect_expr.forall()) {
        forall_variable_ids_.push_back(problem->intern_from_protobuf(var));
    }
}

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
