#include "effect_expression.h"
#include <sstream>

namespace planmt {

EffectExpression::EffectExpression(const pb::EffectExpression& pb_effect_expr) 
    : kind_(static_cast<Kind>(pb_effect_expr.kind())),
      fluent_(pb_effect_expr.fluent()),
      value_(pb_effect_expr.value()) {
    
    if (pb_effect_expr.has_condition()) {
        condition_ = Expression(pb_effect_expr.condition());
    }
    
    for (const auto& var : pb_effect_expr.forall()) {
        forall_variables_.emplace_back(var);
    }
}

std::string EffectExpression::to_string() const {
    std::ostringstream oss;
    
    // Add forall quantifiers if present
    if (is_quantified()) {
        oss << "(forall (";
        for (size_t i = 0; i < forall_variables_.size(); ++i) {
            if (i > 0) oss << " ";
            oss << forall_variables_[i].to_string();
        }
        oss << ") ";
    }
    
    // Add condition if present
    if (is_conditional()) {
        oss << "(when " << condition_->to_string() << " ";
    }
    
    // Add the effect
    oss << "(" << kind_to_string() << " " << fluent_.to_string() << " " << value_.to_string() << ")";
    
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

pb::EffectExpression EffectExpression::to_protobuf() const {
    pb::EffectExpression pb_effect_expr;
    
    pb_effect_expr.set_kind(static_cast<pb::EffectExpression_EffectKind>(kind_));
    *pb_effect_expr.mutable_fluent() = fluent_.to_protobuf();
    *pb_effect_expr.mutable_value() = value_.to_protobuf();
    
    if (condition_.has_value()) {
        *pb_effect_expr.mutable_condition() = condition_->to_protobuf();
    }
    
    for (const auto& var : forall_variables_) {
        *pb_effect_expr.add_forall() = var.to_protobuf();
    }
    
    return pb_effect_expr;
}

bool EffectExpression::operator==(const EffectExpression& other) const {
    return kind_ == other.kind_ &&
           fluent_ == other.fluent_ &&
           value_ == other.value_ &&
           condition_ == other.condition_ &&
           forall_variables_ == other.forall_variables_;
}

} // namespace planmt
