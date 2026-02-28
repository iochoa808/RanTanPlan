#pragma once

#include <string>

namespace rantanplan {

/**
 * @brief Expression structural kinds
 */
enum class ExprKind {
    UNKNOWN = 0,                // Default value, should not be used
    CONSTANT = 1,               // Constant atom (e.g., `true`, `3`, `kitchen`)
    PARAMETER = 2,              // Parameter from outer scope (e.g., `from` in `(move from to)`)
    FLUENT_SYMBOL = 3,          // Fluent symbol (e.g., `at-robot`)
    FUNCTION_SYMBOL = 4,        // Function symbol (e.g., `+`, `=`, `and`)
    STATE_VARIABLE = 5,         // Fluent application (e.g., `(at-robot l1)`)
    FUNCTION_APPLICATION = 6,   // Function application (e.g., `(+ 1 3)`)
    VARIABLE = 7                // Variable from outer scope (existential/universal)
    // NOTE: Protobuf ExpressionKind also defines CONTAINER_ID = 8.
    // This internal IR does not support it; parsing rejects it explicitly in
    // Problem::intern_from_protobuf() with a runtime error.
};

/**
 * @brief Operator types for type safety and consistency
 */
enum class ExprOperator {
    // Arithmetic operators
    PLUS, MINUS, MULTIPLY, DIVIDE, MODULO, ABSOLUTE, MAXIMUM, MINIMUM,

    // Comparison operators
    EQUALS, LESS_EQUAL, LESS_THAN, GREATER_EQUAL, GREATER_THAN,

    // Logical operators
    AND, OR, NOT, IMPLIES, IFF,

    // Unknown operator
    UNKNOWN
};

// ============================================================================
// Static utility methods (formerly on Expression)
// ============================================================================

inline ExprOperator string_to_expr_operator(const std::string& op_string) {
    // UP notation
    if (op_string == "up:plus") return ExprOperator::PLUS;
    if (op_string == "up:minus") return ExprOperator::MINUS;
    if (op_string == "up:times") return ExprOperator::MULTIPLY;
    if (op_string == "up:div") return ExprOperator::DIVIDE;
    if (op_string == "up:mod") return ExprOperator::MODULO;
    if (op_string == "up:abs") return ExprOperator::ABSOLUTE;
    if (op_string == "up:max") return ExprOperator::MAXIMUM;
    if (op_string == "up:min") return ExprOperator::MINIMUM;
    if (op_string == "up:equals") return ExprOperator::EQUALS;
    if (op_string == "up:le") return ExprOperator::LESS_EQUAL;
    if (op_string == "up:lt") return ExprOperator::LESS_THAN;
    if (op_string == "up:ge") return ExprOperator::GREATER_EQUAL;
    if (op_string == "up:gt") return ExprOperator::GREATER_THAN;
    if (op_string == "up:and") return ExprOperator::AND;
    if (op_string == "up:or") return ExprOperator::OR;
    if (op_string == "up:not") return ExprOperator::NOT;
    if (op_string == "up:implies") return ExprOperator::IMPLIES;
    if (op_string == "up:iff") return ExprOperator::IFF;

    // Standard notation
    if (op_string == "+") return ExprOperator::PLUS;
    if (op_string == "-") return ExprOperator::MINUS;
    if (op_string == "*") return ExprOperator::MULTIPLY;
    if (op_string == "/") return ExprOperator::DIVIDE;
    if (op_string == "mod") return ExprOperator::MODULO;
    if (op_string == "abs") return ExprOperator::ABSOLUTE;
    if (op_string == "max") return ExprOperator::MAXIMUM;
    if (op_string == "min") return ExprOperator::MINIMUM;
    if (op_string == "=" || op_string == "==") return ExprOperator::EQUALS;
    if (op_string == "<=") return ExprOperator::LESS_EQUAL;
    if (op_string == "<") return ExprOperator::LESS_THAN;
    if (op_string == ">=") return ExprOperator::GREATER_EQUAL;
    if (op_string == ">") return ExprOperator::GREATER_THAN;
    if (op_string == "and") return ExprOperator::AND;
    if (op_string == "or") return ExprOperator::OR;
    if (op_string == "not") return ExprOperator::NOT;
    if (op_string == "=>") return ExprOperator::IMPLIES;
    if (op_string == "<=>") return ExprOperator::IFF;

    return ExprOperator::UNKNOWN;
}

inline std::string expr_operator_to_string(ExprOperator op) {
    switch (op) {
        case ExprOperator::PLUS: return "+";
        case ExprOperator::MINUS: return "-";
        case ExprOperator::MULTIPLY: return "*";
        case ExprOperator::DIVIDE: return "/";
        case ExprOperator::MODULO: return "mod";
        case ExprOperator::ABSOLUTE: return "abs";
        case ExprOperator::MAXIMUM: return "max";
        case ExprOperator::MINIMUM: return "min";
        case ExprOperator::EQUALS: return "=";
        case ExprOperator::LESS_EQUAL: return "<=";
        case ExprOperator::LESS_THAN: return "<";
        case ExprOperator::GREATER_EQUAL: return ">=";
        case ExprOperator::GREATER_THAN: return ">";
        case ExprOperator::AND: return "and";
        case ExprOperator::OR: return "or";
        case ExprOperator::NOT: return "not";
        case ExprOperator::IMPLIES: return "=>";
        case ExprOperator::IFF: return "=";
        default: return "";
    }
}

inline std::string map_up_operator(const std::string& up_function_name) {
    ExprOperator op = string_to_expr_operator(up_function_name);
    if (op == ExprOperator::UNKNOWN) {
        return up_function_name;
    }
    return expr_operator_to_string(op);
}

inline bool is_arithmetic_operator(ExprOperator op) {
    return op == ExprOperator::PLUS || op == ExprOperator::MINUS ||
           op == ExprOperator::MULTIPLY || op == ExprOperator::DIVIDE ||
           op == ExprOperator::MODULO || op == ExprOperator::ABSOLUTE ||
           op == ExprOperator::MAXIMUM || op == ExprOperator::MINIMUM;
}

inline bool is_comparison_operator(ExprOperator op) {
    return op == ExprOperator::EQUALS || op == ExprOperator::LESS_EQUAL ||
           op == ExprOperator::LESS_THAN || op == ExprOperator::GREATER_EQUAL ||
           op == ExprOperator::GREATER_THAN;
}

inline bool is_logical_operator(ExprOperator op) {
    return op == ExprOperator::AND || op == ExprOperator::OR ||
           op == ExprOperator::NOT || op == ExprOperator::IMPLIES ||
           op == ExprOperator::IFF;
}

inline bool is_standard_operator(const std::string& op) {
    return string_to_expr_operator(op) != ExprOperator::UNKNOWN;
}

} // namespace rantanplan
