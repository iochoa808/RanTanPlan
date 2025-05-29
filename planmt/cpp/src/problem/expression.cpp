#include "expression.h"
#include <sstream>

namespace planmt {

Expression::Expression(const pb::Expression& pb_expression) {
    // Set type
    type_ = pb_expression.type();
    
    // Convert kind
    kind_ = static_cast<Kind>(pb_expression.kind());
    
    // Check if atom is present
    if (pb_expression.has_atom()) {
        atom_ = Atom(pb_expression.atom());
    }
    
    // Check if list is present
    if (pb_expression.list_size() > 0) {
        for (const auto& pb_expr : pb_expression.list()) {
            list_.emplace_back(pb_expr);
        }
    }
}

std::string Expression::to_string() const {
    std::ostringstream oss;
    
    if (is_atom()) {
        oss << atom_->to_string();
    } else if (is_list()) {
        oss << list_to_string();
    } else {
        oss << "EMPTY";
    }
    
    return oss.str();
}

std::string Expression::list_to_string() const {
    if (list_.empty()) {
        return "()";
    }
    
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < list_.size(); ++i) {
        if (i > 0) oss << " ";
        oss << list_[i].to_string();
    }
    oss << ")";
    return oss.str();
}

pb::Expression Expression::to_protobuf() const {
    pb::Expression pb_expression;
    
    // Set type and kind
    pb_expression.set_type(type_);
    pb_expression.set_kind(static_cast<pb::ExpressionKind>(kind_));
    
    if (is_atom()) {
        *pb_expression.mutable_atom() = atom_->to_protobuf();
    } else if (is_list()) {
        for (const auto& expr : list_) {
            *pb_expression.add_list() = expr.to_protobuf();
        }
    }
    
    return pb_expression;
}

bool Expression::operator==(const Expression& other) const {
    if (kind_ != other.kind_ || type_ != other.type_) {
        return false;
    }
    
    if (is_atom() && other.is_atom()) {
        return atom_ == other.atom_;
    } else if (is_list() && other.is_list()) {
        return list_ == other.list_;
    }
    
    return false;
}

} // namespace planmt
