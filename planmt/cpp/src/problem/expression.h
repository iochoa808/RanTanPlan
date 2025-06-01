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
 * Represents either an atom or a list of sub-expressions.
 */
class Expression {
public:
    // Expression kinds. For some reason the protobuf enum has weird numbering.
    enum class Kind {
        // Default value, should not be used. Drop it if we are sure to never need it.
        UNKNOWN = 0, 

        // Constant atom. For instance, `true`, `3` or `kitchen` (where `kitchen` is
        // an object defined in the problem)
        CONSTANT = 1, 

        // Atom symbol representing a parameter from an outer scope. For instance
        // `from` that would appear inside a `(move from to - location)` action.
        PARAMETER = 2,           

        // Atom symbol representing a variable from an outer scope.
        // This is typically used to represent the variables that are existentially or universally qualified in expressions.
        VARIABLE = 7,

        // Atom symbol representing a fluent of the problem. For instance `at-robot`.
        FLUENT_SYMBOL = 3,

        // Atom representing a function. For instance `+`, `=`, `and`, ...
        FUNCTION_SYMBOL = 4, 

        // List. Application of some parameters to a fluent symbol. For instance `(at-robot l1)` or `(battery-charged)`
        // The first element of the list must be a FLUENT_SYMBOL
        STATE_VARIABLE = 5,

        // List. The expression is the application of some parameters to a function. For instance `(+ 1 3)`.
        // The first element of the list must be a FUNCTION_SYMBOL
        FUNCTION_APPLICATION = 6
    };
    
    // Constructors
    Expression() : kind_(Kind::UNKNOWN) {}
    Expression(const pb::Expression& pb_expression);
    
    // checkers
    bool is_atom() const { return atom_.has_value() && list_.empty(); }
    bool is_list() const { return !list_.empty(); }
    
    // getters
    const Atom& value() const { return atom_.value(); }

    Kind kind() const { return kind_; }
    const std::string& type() const { return type_; }

    size_t list_size() const { return list_.size(); }
    const std::vector<Expression>& list() const { return list_; }
    const Expression& list_element(size_t index) const { return list_[index]; }
    
    // Convenience methods for common expression types
    bool is_constant() const { return kind_ == Kind::CONSTANT; }
    bool is_parameter() const { return kind_ == Kind::PARAMETER; }
    bool is_variable() const { return kind_ == Kind::VARIABLE; }
    bool is_fluent_symbol() const { return kind_ == Kind::FLUENT_SYMBOL; }
    bool is_function_symbol() const { return kind_ == Kind::FUNCTION_SYMBOL; }
    bool is_state_variable() const { return kind_ == Kind::STATE_VARIABLE; }
    bool is_function_application() const { return kind_ == Kind::FUNCTION_APPLICATION; }
    
    // Convenience methods for common operators
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
    
    // String representation
    std::string to_string() const;
    
    // Operators
    bool operator==(const Expression& other) const;
    bool operator!=(const Expression& other) const { return !(*this == other); }

    // Operator enum for type safety and consistency
    enum class Operator {
        // Arithmetic operators
        PLUS,           // + / up:plus
        MINUS,          // - / up:minus
        MULTIPLY,       // * / up:times
        DIVIDE,         // / / up:div
        MODULO,         // mod / up:mod
        ABSOLUTE,       // abs / up:abs
        MAXIMUM,        // max / up:max
        MINIMUM,        // min / up:min
        
        // Comparison operators
        EQUALS,         // = / up:equals
        LESS_EQUAL,     // <= / up:le
        LESS_THAN,      // < / up:lt
        GREATER_EQUAL,  // >= / up:ge
        GREATER_THAN,   // > / up:gt
        
        // Logical operators
        AND,            // and / up:and
        OR,             // or / up:or
        NOT,            // not / up:not
        IMPLIES,        // => / up:implies
        IFF,            // = / up:iff
        
        // Unknown operator
        UNKNOWN
    };

    // Static utility methods for operator handling
    /**
     * @brief Converts UP notation string to Operator enum
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
     * @param up_function_name The UP notation function name (e.g., "up:plus")
     * @return The standard operator symbol (e.g., "+") or original name if not found
     */
    static std::string map_up_operator(const std::string& up_function_name) {
        Operator op = string_to_operator(up_function_name);
        if (op == Operator::UNKNOWN) {
            return up_function_name;  // Return original if not found
        }
        return operator_to_string(op);
    }

    /**
     * @brief Checks if an Operator is an arithmetic operator
     */
    static bool is_arithmetic_operator(Operator op) {
        return op == Operator::PLUS || op == Operator::MINUS || 
               op == Operator::MULTIPLY || op == Operator::DIVIDE || 
               op == Operator::MODULO || op == Operator::ABSOLUTE ||
               op == Operator::MAXIMUM || op == Operator::MINIMUM;
    }

    /**
     * @brief Checks if an Operator is a comparison operator
     */
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

    /**
     * @brief Checks if a string represents a standard arithmetic operator
     */
    static bool is_arithmetic_operator(const std::string& op) {
        return is_arithmetic_operator(string_to_operator(op));
    }

    /**
     * @brief Checks if a string represents a standard comparison operator
     */
    static bool is_comparison_operator(const std::string& op) {
        return is_comparison_operator(string_to_operator(op));
    }

    /**
     * @brief Checks if a string represents a standard logical operator
     */
    static bool is_logical_operator(const std::string& op) {
        return is_logical_operator(string_to_operator(op));
    }

    /**
     * @brief Checks if a string represents any standard operator
     */
    static bool is_standard_operator(const std::string& op) {
        return string_to_operator(op) != Operator::UNKNOWN;
    }

private:
    std::optional<Atom> atom_;
    std::vector<Expression> list_;

    Kind kind_;        // structural information (such as whether it is a function application, atom, etc.)
    std::string type_; // for example, "up:bool", "up:integer" or custom such as "location", "robot", etc ...
};

} // namespace planmt
