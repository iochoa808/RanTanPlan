#include "expression.h"
#include "problem.h"
#include <sstream>

namespace planmt {

Expression::Expression(const pb::Expression& pb_expression, const Problem* problem) {
    // Set type from protobuf and resolve using Problem context
    const Type* resolved_type = resolve_type(pb_expression.type(), problem);
    type_ = resolved_type;
    
    // Convert kind
    kind_ = static_cast<Kind>(pb_expression.kind());
    
    // Handle atom vs list based on the protobuf content
    if (pb_expression.has_atom()) {
        // This is an atom expression
        Atom original_atom(pb_expression.atom());
        
        // If this is a symbol and it's a function symbol, apply UP operator mapping
        if (original_atom.is_symbol() && 
            (kind_ == Kind::FUNCTION_SYMBOL || kind_ == Kind::FLUENT_SYMBOL)) {
            std::string mapped_symbol = Expression::map_up_operator(original_atom.symbol());
            // Create a new atom with the mapped symbol
            pb::Atom mapped_pb_atom;
            mapped_pb_atom.set_symbol(mapped_symbol);
            atom_ = Atom(mapped_pb_atom);
        } else {
            atom_ = original_atom;
        }
    }
    
    // Handle list (for function applications, etc.)
    if (pb_expression.list_size() > 0) {
        list_.clear();
        for (int i = 0; i < pb_expression.list_size(); ++i) {
            const auto& pb_expr = pb_expression.list(i);
            
            // For the first element in function applications, apply operator mapping
            if (i == 0 && kind_ == Kind::FUNCTION_APPLICATION && pb_expr.has_atom() && pb_expr.atom().has_symbol()) {
                // Create a modified protobuf expression with mapped operator
                pb::Expression modified_pb_expr = pb_expr;
                std::string original_symbol = pb_expr.atom().symbol();
                std::string mapped_symbol = Expression::map_up_operator(original_symbol);
                modified_pb_expr.mutable_atom()->set_symbol(mapped_symbol);
                list_.emplace_back(modified_pb_expr, problem);
            } else {
                // Recursively process other elements
                list_.emplace_back(pb_expr, problem);
            }
        }
    }
}

const Type* Expression::resolve_type(const std::string& type_str, const Problem* problem) const {
    if (type_str.empty()) {
        return nullptr;
    }
    
    // If we have a Problem context, try to find the type
    if (problem) {
        const Type* found_type = problem->find_type(type_str);
        if (found_type) {
            return found_type;
        }
    }
    
    // For primitive types, we might need to create/find them in a global registry
    // For now, return nullptr if not found in problem context
    return nullptr;
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

bool Expression::is_equals() const {
    return value().symbol() == "=" || value().symbol() == "==";
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
} // namespace planmt
