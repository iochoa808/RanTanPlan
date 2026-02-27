#include "effect_expression.hpp"
#include "problem.hpp"
#include <sstream>

namespace rantanplan {

EffectExpression::EffectExpression(const pb::EffectExpression& pb_effect_expr, Problem* problem)
    : kind_(static_cast<Kind>(pb_effect_expr.kind())) {

    pool_ = &problem->pool();

    fluent_id_ = problem->intern_from_protobuf(pb_effect_expr.fluent());
    value_id_ = problem->intern_from_protobuf(pb_effect_expr.value());

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

    // Add the effect
    if (pool_) {
        oss << "(" << kind_to_string() << " " << pool_->to_string(fluent_id_) << " " << pool_->to_string(value_id_) << ")";
    } else {
        oss << "(" << kind_to_string() << " eid:" << fluent_id_.id << " eid:" << value_id_.id << ")";
    }

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
