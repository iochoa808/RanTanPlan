#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "protobuf_aliases.h"
#include "atom.h"

namespace planmt {

/**
 * @brief Expression
 * 
 * Represents either an atom or a list of sub-expressions.
 */
class Expression {
public:
    // Expression kinds
    enum class Kind {
        UNKNOWN = 0,
        CONSTANT = 1,            // Constant atom
        PARAMETER = 2,           // Parameter from outer scope
        VARIABLE = 3,            // Variable from outer scope
        FLUENT_SYMBOL = 4,       // Fluent symbol
        FUNCTION_SYMBOL = 5,     // Function symbol
        STATE_VARIABLE = 6,      // Fluent application
        FUNCTION_APPLICATION = 7 // Function application
    };
    
    // Constructors
    Expression() : kind_(Kind::UNKNOWN) {}
    Expression(const Atom& atom, Kind kind = Kind::CONSTANT) 
        : atom_(atom), kind_(kind) {}
    Expression(const std::vector<Expression>& list, Kind kind = Kind::FUNCTION_APPLICATION)
        : list_(list), kind_(kind) {}
    Expression(const pb::Expression& pb_expression);
    
    // Type checkers
    bool is_atom() const { return atom_.has_value() && list_.empty(); }
    bool is_list() const { return !list_.empty(); }
    
    // Value getters
    const Atom& atom() const { return atom_.value(); }
    const std::vector<Expression>& list() const { return list_; }
    Kind kind() const { return kind_; }
    const std::string& type() const { return type_; }
    
    // Setters
    void set_atom(const Atom& atom) { atom_ = atom; list_.clear(); }
    void set_list(const std::vector<Expression>& list) { list_ = list; atom_.reset(); }
    void set_kind(Kind kind) { kind_ = kind; }
    void set_type(const std::string& type) { type_ = type; }
    
    // List manipulation
    void add_expression(const Expression& expr) { list_.push_back(expr); }
    size_t list_size() const { return list_.size(); }
    const Expression& list_element(size_t index) const { return list_[index]; }
    
    // Convenience methods for common expression types
    bool is_constant() const { return kind_ == Kind::CONSTANT; }
    bool is_parameter() const { return kind_ == Kind::PARAMETER; }
    bool is_variable() const { return kind_ == Kind::VARIABLE; }
    bool is_fluent_symbol() const { return kind_ == Kind::FLUENT_SYMBOL; }
    bool is_function_symbol() const { return kind_ == Kind::FUNCTION_SYMBOL; }
    bool is_state_variable() const { return kind_ == Kind::STATE_VARIABLE; }
    bool is_function_application() const { return kind_ == Kind::FUNCTION_APPLICATION; }
    
    // Operator type identification for function applications
    enum class OperatorType {
        UNKNOWN,
        // Logical operators
        AND, OR, NOT, IMPLIES, IFF, EXISTS, FORALL,
        // Comparison operators  
        EQUALS, NOT_EQUALS, LESS_THAN, LESS_EQUAL, GREATER_THAN, GREATER_EQUAL,
        // Arithmetic operators
        PLUS, MINUS, MULTIPLY, DIVIDE, MODULO, POWER,
        // Custom/unknown operator
        CUSTOM
    };

    // Get operator type for function applications and function symbols
    OperatorType get_operator_type() const;
    
    // Get operator symbol (for function symbols and applications)
    std::string get_operator_symbol() const;
    
    // Check if this is a specific category of operator
    bool is_logical_operator() const;
    bool is_comparison_operator() const; 
    bool is_arithmetic_operator() const;
    
    // Convenience methods for common operators
    bool is_and() const { return get_operator_type() == OperatorType::AND; }
    bool is_or() const { return get_operator_type() == OperatorType::OR; }
    bool is_not() const { return get_operator_type() == OperatorType::NOT; }
    bool is_equals() const { return get_operator_type() == OperatorType::EQUALS; }
    bool is_plus() const { return get_operator_type() == OperatorType::PLUS; }
    bool is_minus() const { return get_operator_type() == OperatorType::MINUS; }
    bool is_multiply() const { return get_operator_type() == OperatorType::MULTIPLY; }
    bool is_divide() const { return get_operator_type() == OperatorType::DIVIDE; }
    bool is_less_than() const { return get_operator_type() == OperatorType::LESS_THAN; }
    bool is_greater_than() const { return get_operator_type() == OperatorType::GREATER_THAN; }
    
    // Static utility methods for operator symbol mapping
    static OperatorType symbol_to_operator_type(const std::string& symbol);
    static std::string operator_type_to_symbol(OperatorType type);
    static bool is_logical_operator_type(OperatorType type);
    static bool is_comparison_operator_type(OperatorType type);
    static bool is_arithmetic_operator_type(OperatorType type);
    
    // String representation
    std::string to_string() const;
    
    // Operators
    bool operator==(const Expression& other) const;
    bool operator!=(const Expression& other) const { return !(*this == other); }

private:
    std::optional<Atom> atom_;
    std::vector<Expression> list_;
    Kind kind_;
    std::string type_;
    
    // Helper for string conversion
    std::string list_to_string() const;
};

// Operator overload for printing Kind enum
inline std::ostream& operator<<(std::ostream& os, const Expression::Kind& kind) {
    switch (kind) {
        case Expression::Kind::UNKNOWN: return os << "UNKNOWN";
        case Expression::Kind::CONSTANT: return os << "CONSTANT";
        case Expression::Kind::PARAMETER: return os << "PARAMETER";
        case Expression::Kind::VARIABLE: return os << "VARIABLE";
        case Expression::Kind::FLUENT_SYMBOL: return os << "FLUENT_SYMBOL";
        case Expression::Kind::FUNCTION_SYMBOL: return os << "FUNCTION_SYMBOL";
        case Expression::Kind::STATE_VARIABLE: return os << "STATE_VARIABLE";
        case Expression::Kind::FUNCTION_APPLICATION: return os << "FUNCTION_APPLICATION";
        default: return os << "UNKNOWN";
    }
}

} // namespace planmt
