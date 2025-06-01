#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include "protobuf_aliases.h"
#include "atom.h"

namespace planmt {

/**
 * @brief Expression
 * 
 * Represents either an atom or a list of sub-expressions in planning problems.
 * Provides comprehensive type safety and operator handling for planning expressions.
 */
class Expression {
public:
    // ========================================================================
    // ENUMS
    // ========================================================================
    
    /**
     * @brief Expression structural kinds
     */
    enum class Kind {
        UNKNOWN = 0,                // Default value, should not be used
        CONSTANT = 1,               // Constant atom (e.g., `true`, `3`, `kitchen`)
        PARAMETER = 2,              // Parameter from outer scope (e.g., `from` in `(move from to)`)
        FLUENT_SYMBOL = 3,          // Fluent symbol (e.g., `at-robot`)
        FUNCTION_SYMBOL = 4,        // Function symbol (e.g., `+`, `=`, `and`)
        STATE_VARIABLE = 5,         // Fluent application (e.g., `(at-robot l1)`)
        FUNCTION_APPLICATION = 6,   // Function application (e.g., `(+ 1 3)`)
        VARIABLE = 7                // Variable from outer scope (existential/universal)
    };
    
    /**
     * @brief Expression data types
     */
    enum class Type {
        BOOLEAN,     // up:bool
        INTEGER,     // up:integer  
        REAL,        // up:real
        OBJECT,      // Custom PDDL types (aircraft, city, person, etc.)
        UNKNOWN      // Unknown or unspecified type
    };

    /**
     * @brief Operator types for type safety and consistency
     */
    enum class Operator {
        // Arithmetic operators
        PLUS, MINUS, MULTIPLY, DIVIDE, MODULO, ABSOLUTE, MAXIMUM, MINIMUM,
        
        // Comparison operators
        EQUALS, LESS_EQUAL, LESS_THAN, GREATER_EQUAL, GREATER_THAN,
        
        // Logical operators
        AND, OR, NOT, IMPLIES, IFF,
        
        // Unknown operator
        UNKNOWN
    };

    // ========================================================================
    // STATIC UTILITY METHODS
    // ========================================================================
    
    /**
     * @brief Converts type string to Type enum
     */
    static Type string_to_type(const std::string& type_str) {
        if (type_str == "up:bool") return Type::BOOLEAN;
        if (type_str == "up:integer") return Type::INTEGER;
        if (type_str == "up:real") return Type::REAL;
        if (type_str.empty() || type_str == "unknown") return Type::UNKNOWN;
        return Type::OBJECT; // All other types are OBJECT types
    }

    /**
     * @brief Converts operator string to Operator enum
     */
    static Operator string_to_operator(const std::string& op_string) {
        // UP notation
        if (op_string == "up:plus") return Operator::PLUS;
        if (op_string == "up:minus") return Operator::MINUS;
        if (op_string == "up:times") return Operator::MULTIPLY;
        if (op_string == "up:div") return Operator::DIVIDE;
        if (op_string == "up:mod") return Operator::MODULO;
        if (op_string == "up:abs") return Operator::ABSOLUTE;
        if (op_string == "up:max") return Operator::MAXIMUM;
        if (op_string == "up:min") return Operator::MINIMUM;
        if (op_string == "up:equals") return Operator::EQUALS;
        if (op_string == "up:le") return Operator::LESS_EQUAL;
        if (op_string == "up:lt") return Operator::LESS_THAN;
        if (op_string == "up:ge") return Operator::GREATER_EQUAL;
        if (op_string == "up:gt") return Operator::GREATER_THAN;
        if (op_string == "up:and") return Operator::AND;
        if (op_string == "up:or") return Operator::OR;
        if (op_string == "up:not") return Operator::NOT;
        if (op_string == "up:implies") return Operator::IMPLIES;
        if (op_string == "up:iff") return Operator::IFF;
        
        // Standard notation
        if (op_string == "+") return Operator::PLUS;
        if (op_string == "-") return Operator::MINUS;
        if (op_string == "*") return Operator::MULTIPLY;
        if (op_string == "/") return Operator::DIVIDE;
        if (op_string == "mod") return Operator::MODULO;
        if (op_string == "abs") return Operator::ABSOLUTE;
        if (op_string == "max") return Operator::MAXIMUM;
        if (op_string == "min") return Operator::MINIMUM;
        if (op_string == "=" || op_string == "==") return Operator::EQUALS;
        if (op_string == "<=") return Operator::LESS_EQUAL;
        if (op_string == "<") return Operator::LESS_THAN;
        if (op_string == ">=") return Operator::GREATER_EQUAL;
        if (op_string == ">") return Operator::GREATER_THAN;
        if (op_string == "and") return Operator::AND;
        if (op_string == "or") return Operator::OR;
        if (op_string == "not") return Operator::NOT;
        if (op_string == "=>") return Operator::IMPLIES;
        if (op_string == "=" || op_string == "<=>") return Operator::IFF;
        
        return Operator::UNKNOWN;
    }

    /**
     * @brief Converts Operator enum to standard notation string
     */
    static std::string operator_to_string(Operator op) {
        switch (op) {
            case Operator::PLUS: return "+";
            case Operator::MINUS: return "-";
            case Operator::MULTIPLY: return "*";
            case Operator::DIVIDE: return "/";
            case Operator::MODULO: return "mod";
            case Operator::ABSOLUTE: return "abs";
            case Operator::MAXIMUM: return "max";
            case Operator::MINIMUM: return "min";
            case Operator::EQUALS: return "=";
            case Operator::LESS_EQUAL: return "<=";
            case Operator::LESS_THAN: return "<";
            case Operator::GREATER_EQUAL: return ">=";
            case Operator::GREATER_THAN: return ">";
            case Operator::AND: return "and";
            case Operator::OR: return "or";
            case Operator::NOT: return "not";
            case Operator::IMPLIES: return "=>";
            case Operator::IFF: return "=";
            default: return "";
        }
    }

    /**
     * @brief Maps UP notation operators to standard mathematical symbols
     */
    static std::string map_up_operator(const std::string& up_function_name) {
        Operator op = string_to_operator(up_function_name);
        if (op == Operator::UNKNOWN) {
            return up_function_name;  // Return original if not found
        }
        return operator_to_string(op);
    }

    static bool is_arithmetic_operator(Operator op) {
        return op == Operator::PLUS || op == Operator::MINUS || 
               op == Operator::MULTIPLY || op == Operator::DIVIDE || 
               op == Operator::MODULO || op == Operator::ABSOLUTE ||
               op == Operator::MAXIMUM || op == Operator::MINIMUM;
    }

    static bool is_comparison_operator(Operator op) {
        return op == Operator::EQUALS || op == Operator::LESS_EQUAL || 
               op == Operator::LESS_THAN || op == Operator::GREATER_EQUAL || 
               op == Operator::GREATER_THAN;
    }

    /**
     * @brief Checks if an Operator is a logical operator
     */
    static bool is_logical_operator(Operator op) {
        return op == Operator::AND || op == Operator::OR || 
               op == Operator::NOT || op == Operator::IMPLIES || 
               op == Operator::IFF;
    }

    static bool is_arithmetic_operator(const std::string& op) {
        return is_arithmetic_operator(string_to_operator(op));
    }

    static bool is_comparison_operator(const std::string& op) {
        return is_comparison_operator(string_to_operator(op));
    }

    static bool is_logical_operator(const std::string& op) {
        return is_logical_operator(string_to_operator(op));
    }

    static bool is_standard_operator(const std::string& op) {
        return string_to_operator(op) != Operator::UNKNOWN;
    }
    
    // ========================================================================
    // CONSTRUCTORS
    // ========================================================================
    
    Expression() : kind_(Kind::UNKNOWN) {}
    Expression(const pb::Expression& pb_expression);
    
    // ========================================================================
    // CORE ACCESSORS
    // ========================================================================
    
    /**
     * @brief Get the atom value (only valid if is_atom() returns true)
     */
    const Atom& value() const { return atom_.value(); }

    /**
     * @brief Get the structural kind of this expression
     */
    Kind kind() const { return kind_; }
    
    /**
     * @brief Get the type string (e.g., "up:bool", "up:integer", "location")
     */
    const std::string& type() const { return type_; }
    
    /**
     * @brief Get type enum from type string
     */
    Type type_enum() const { return string_to_type(type_); }

    /**
     * @brief Get the number of elements in the list
     */
    size_t list_size() const { return list_.size(); }
    
    /**
     * @brief Get the entire list of sub-expressions
     */
    const std::vector<Expression>& list() const { return list_; }
    
    /**
     * @brief Get a specific element from the list
     */
    const Expression& list_element(size_t index) const { return list_[index]; }
    
    // ========================================================================
    // STRUCTURAL TYPE CHECKING
    // ========================================================================
    
    bool is_atom() const { return atom_.has_value() && list_.empty(); }
    bool is_list() const { return !list_.empty(); }
    bool is_constant() const { return kind_ == Kind::CONSTANT; }
    bool is_parameter() const { return kind_ == Kind::PARAMETER; }
    bool is_variable() const { return kind_ == Kind::VARIABLE; }
    bool is_fluent_symbol() const { return kind_ == Kind::FLUENT_SYMBOL; }
    bool is_function_symbol() const { return kind_ == Kind::FUNCTION_SYMBOL; }
    bool is_state_variable() const { return kind_ == Kind::STATE_VARIABLE; }
    bool is_function_application() const { return kind_ == Kind::FUNCTION_APPLICATION; }
    
    // ========================================================================
    // OPERATOR-SPECIFIC CHECKING
    // ========================================================================

    bool is_and() const;
    bool is_or() const;
    bool is_not() const;
    bool is_implies() const;
    bool is_iff() const;
    bool is_exists() const;
    bool is_forall() const;
    bool is_equals() const;
    bool is_not_equals() const;
    bool is_plus() const;
    bool is_minus() const;
    bool is_multiply() const;
    bool is_divide() const;
    bool is_less_than() const;
    bool is_less_equal() const;
    bool is_greater_than() const;
    bool is_greater_equal() const;
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    /**
     * @brief Get string representation of this expression
     */
    std::string to_string() const;
    bool operator==(const Expression& other) const;
    bool operator!=(const Expression& other) const { return !(*this == other); }

private:
    // ========================================================================
    // PRIVATE MEMBERS
    // ========================================================================
    
    std::optional<Atom> atom_;   ///< Atom value (if this expression is an atom)
    std::vector<Expression> list_;  ///< List of sub-expressions (if this expression is a list)

    Kind kind_;        ///< Structural information (function application, atom, etc.)
    std::string type_; ///< Type information ("up:bool", "up:integer", custom types, etc.)
};

} // namespace planmt
