#include "expression.h"
#include <sstream>

namespace planmt {

Expression::Expression(const pb::Expression& pb_expression) {
    // Set type
    type_ = pb_expression.type();
    
    // Convert kind
    kind_ = static_cast<Kind>(pb_expression.kind());
    
    // get the atom
    assert(pb_expression.has_atom());
    atom_ = Atom(pb_expression.atom());
    
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
        if (list_.empty()) {
            oss << "()";
        } else {
            oss << "(";
            for (size_t i = 0; i < list_.size(); ++i) {
                if (i > 0) oss << " ";
                oss << list_[i].to_string();
            }
            oss << ")";
        }
    } else {
        oss << "EMPTY";
    }
    
    return oss.str();
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

bool Expression::is_and() const {
    return value().symbol() == "and";
}

bool Expression::is_or() const {
    return value().symbol() == "or";
}

bool Expression::is_not() const {
    return value().symbol() == "not";
}

bool Expression::is_implies() const {
    return value().symbol() == "implies" || value().symbol() == "=>";
}

bool Expression::is_iff() const {
    return value().symbol() == "iff" || value().symbol() == "<=>";
}

bool Expression::is_exists() const {
    return value().symbol() == "exists";
}

bool Expression::is_forall() const {
    return value().symbol() == "forall";
}

bool Expression::is_equals() const {
    return value().symbol() == "=" || value().symbol() == "==";
}

bool Expression::is_not_equals() const {
    return value().symbol() == "!=" || value().symbol() == "<>";
}

bool Expression::is_less_than() const {
    return value().symbol() == "<";
}

bool Expression::is_less_equal() const {
    return value().symbol() == "<=";
}

bool Expression::is_greater_than() const {
    return value().symbol() == ">";
}

bool Expression::is_greater_equal() const {
    return value().symbol() == ">=";
}

bool Expression::is_plus() const {
    return value().symbol() == "+";
}

bool Expression::is_minus() const {
    return value().symbol() == "-";
}

bool Expression::is_multiply() const {
    return value().symbol() == "*";
}

bool Expression::is_divide() const {
    return value().symbol() == "/";
}
} // namespace planmt
