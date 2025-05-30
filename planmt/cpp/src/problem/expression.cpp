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

// Operator type identification methods
Expression::OperatorType Expression::get_operator_type() const {
    if (is_function_symbol() || is_function_application()) {
        std::string symbol = get_operator_symbol();
        return symbol_to_operator_type(symbol);
    }
    return OperatorType::UNKNOWN;
}

std::string Expression::get_operator_symbol() const {
    if (is_function_symbol() && is_atom()) {
        // For function symbols, get the symbol from the atom
        if (atom_->is_symbol()) {
            return atom_->symbol();
        }
    } else if (is_function_application() && is_list() && !list_.empty()) {
        // For function applications, get the symbol from the first element
        const Expression& first = list_[0];
        if (first.is_function_symbol() && first.is_atom() && first.atom_->is_symbol()) {
            return first.atom_->symbol();
        }
    }
    return "";
}

bool Expression::is_logical_operator() const {
    return is_logical_operator_type(get_operator_type());
}

bool Expression::is_comparison_operator() const {
    return is_comparison_operator_type(get_operator_type());
}

bool Expression::is_arithmetic_operator() const {
    return is_arithmetic_operator_type(get_operator_type());
}

// Static utility methods for operator symbol mapping
Expression::OperatorType Expression::symbol_to_operator_type(const std::string& symbol) {
    // Logical operators
    if (symbol == "and") return OperatorType::AND;
    if (symbol == "or") return OperatorType::OR;
    if (symbol == "not") return OperatorType::NOT;
    if (symbol == "implies" || symbol == "=>") return OperatorType::IMPLIES;
    if (symbol == "iff" || symbol == "<=>") return OperatorType::IFF;
    if (symbol == "exists") return OperatorType::EXISTS;
    if (symbol == "forall") return OperatorType::FORALL;
    
    // Comparison operators
    if (symbol == "=" || symbol == "==") return OperatorType::EQUALS;
    if (symbol == "!=" || symbol == "<>") return OperatorType::NOT_EQUALS;
    if (symbol == "<") return OperatorType::LESS_THAN;
    if (symbol == "<=") return OperatorType::LESS_EQUAL;
    if (symbol == ">") return OperatorType::GREATER_THAN;
    if (symbol == ">=") return OperatorType::GREATER_EQUAL;
    
    // Arithmetic operators
    if (symbol == "+") return OperatorType::PLUS;
    if (symbol == "-") return OperatorType::MINUS;
    if (symbol == "*") return OperatorType::MULTIPLY;
    if (symbol == "/") return OperatorType::DIVIDE;
    if (symbol == "%" || symbol == "mod") return OperatorType::MODULO;
    if (symbol == "^" || symbol == "**") return OperatorType::POWER;
    
    // If not recognized, it's either custom or unknown
    return symbol.empty() ? OperatorType::UNKNOWN : OperatorType::CUSTOM;
}

std::string Expression::operator_type_to_symbol(OperatorType type) {
    switch (type) {
        // Logical operators
        case OperatorType::AND: return "and";
        case OperatorType::OR: return "or";
        case OperatorType::NOT: return "not";
        case OperatorType::IMPLIES: return "implies";
        case OperatorType::IFF: return "iff";
        case OperatorType::EXISTS: return "exists";
        case OperatorType::FORALL: return "forall";
        
        // Comparison operators
        case OperatorType::EQUALS: return "=";
        case OperatorType::NOT_EQUALS: return "!=";
        case OperatorType::LESS_THAN: return "<";
        case OperatorType::LESS_EQUAL: return "<=";
        case OperatorType::GREATER_THAN: return ">";
        case OperatorType::GREATER_EQUAL: return ">=";
        
        // Arithmetic operators
        case OperatorType::PLUS: return "+";
        case OperatorType::MINUS: return "-";
        case OperatorType::MULTIPLY: return "*";
        case OperatorType::DIVIDE: return "/";
        case OperatorType::MODULO: return "%";
        case OperatorType::POWER: return "^";
        
        default: return "";
    }
}

bool Expression::is_logical_operator_type(OperatorType type) {
    switch (type) {
        case OperatorType::AND:
        case OperatorType::OR:
        case OperatorType::NOT:
        case OperatorType::IMPLIES:
        case OperatorType::IFF:
        case OperatorType::EXISTS:
        case OperatorType::FORALL:
            return true;
        default:
            return false;
    }
}

bool Expression::is_comparison_operator_type(OperatorType type) {
    switch (type) {
        case OperatorType::EQUALS:
        case OperatorType::NOT_EQUALS:
        case OperatorType::LESS_THAN:
        case OperatorType::LESS_EQUAL:
        case OperatorType::GREATER_THAN:
        case OperatorType::GREATER_EQUAL:
            return true;
        default:
            return false;
    }
}

bool Expression::is_arithmetic_operator_type(OperatorType type) {
    switch (type) {
        case OperatorType::PLUS:
        case OperatorType::MINUS:
        case OperatorType::MULTIPLY:
        case OperatorType::DIVIDE:
        case OperatorType::MODULO:
        case OperatorType::POWER:
            return true;
        default:
            return false;
    }
}
} // namespace planmt
