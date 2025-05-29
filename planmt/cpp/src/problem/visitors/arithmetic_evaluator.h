#pragma once

#include "expression_visitor.h"
#include <optional>

namespace planmt {

/**
 * @brief Visitor that evaluates simple arithmetic expressions
 * 
 * Can evaluate expressions containing integers and basic arithmetic
 * operations (+, -, *, /). Returns std::nullopt if the expression
 * cannot be evaluated (contains variables, unsupported operations, etc.)
 */
class ArithmeticEvaluator : public BaseExpressionVisitor {
private:
    std::optional<int64_t> result_;
    
public:
    ArithmeticEvaluator() = default;
    
    void visit_integer(int64_t value, Expression::Kind kind) override {
        result_ = value;
    }
    
    void visit_real(const Real& value, Expression::Kind kind) override {
        // For simplicity, we only handle integer arithmetic
        // You could extend this to handle real numbers
        result_ = std::nullopt;
    }
    
    void visit_symbol(const std::string& symbol, Expression::Kind kind) override {
        // Can't evaluate expressions with symbols (unless they're known constants)
        result_ = std::nullopt;
    }
    
    void visit_boolean(bool value, Expression::Kind kind) override {
        // Can't evaluate boolean as arithmetic
        result_ = std::nullopt;
    }
    
    void visit_function_application(const std::string& function_name, 
                                  const std::vector<Expression>& args,
                                  Expression::Kind kind) override {
        if (function_name == "+") {
            evaluate_addition(args);
        } else if (function_name == "-") {
            evaluate_subtraction(args);
        } else if (function_name == "*") {
            evaluate_multiplication(args);
        } else if (function_name == "/") {
            evaluate_division(args);
        } else {
            result_ = std::nullopt; // Unsupported operation
        }
    }
    
    void visit_fluent_application(const std::string& fluent_name,
                                const std::vector<Expression>& args,
                                Expression::Kind kind) override {
        // Can't evaluate fluent applications as arithmetic
        result_ = std::nullopt;
    }
    
    void visit_list(const std::vector<Expression>& elements, 
                   Expression::Kind kind) override {
        // Can't evaluate arbitrary lists as arithmetic
        result_ = std::nullopt;
    }
    
    // Result access
    std::optional<int64_t> get_result() const { return result_; }
    bool has_result() const { return result_.has_value(); }
    int64_t get_value() const { return result_.value(); }
    
private:
    void evaluate_addition(const std::vector<Expression>& args) {
        int64_t sum = 0;
        for (const auto& arg : args) {
            ArithmeticEvaluator sub_evaluator;
            accept_visitor(arg, sub_evaluator);
            if (!sub_evaluator.has_result()) {
                result_ = std::nullopt;
                return;
            }
            sum += sub_evaluator.get_value();
        }
        result_ = sum;
    }
    
    void evaluate_subtraction(const std::vector<Expression>& args) {
        if (args.empty()) {
            result_ = std::nullopt;
            return;
        }
        
        // Unary minus
        if (args.size() == 1) {
            ArithmeticEvaluator sub_evaluator;
            accept_visitor(args[0], sub_evaluator);
            if (sub_evaluator.has_result()) {
                result_ = -sub_evaluator.get_value();
            } else {
                result_ = std::nullopt;
            }
            return;
        }
        
        // Binary subtraction (left-associative for multiple args)
        ArithmeticEvaluator first_evaluator;
        accept_visitor(args[0], first_evaluator);
        if (!first_evaluator.has_result()) {
            result_ = std::nullopt;
            return;
        }
        
        int64_t result = first_evaluator.get_value();
        for (size_t i = 1; i < args.size(); ++i) {
            ArithmeticEvaluator sub_evaluator;
            accept_visitor(args[i], sub_evaluator);
            if (!sub_evaluator.has_result()) {
                result_ = std::nullopt;
                return;
            }
            result -= sub_evaluator.get_value();
        }
        result_ = result;
    }
    
    void evaluate_multiplication(const std::vector<Expression>& args) {
        int64_t product = 1;
        for (const auto& arg : args) {
            ArithmeticEvaluator sub_evaluator;
            accept_visitor(arg, sub_evaluator);
            if (!sub_evaluator.has_result()) {
                result_ = std::nullopt;
                return;
            }
            product *= sub_evaluator.get_value();
        }
        result_ = product;
    }
    
    void evaluate_division(const std::vector<Expression>& args) {
        if (args.size() != 2) {
            result_ = std::nullopt; // Division requires exactly 2 arguments
            return;
        }
        
        ArithmeticEvaluator left_eval, right_eval;
        accept_visitor(args[0], left_eval);
        accept_visitor(args[1], right_eval);
        
        if (left_eval.has_result() && right_eval.has_result()) {
            int64_t divisor = right_eval.get_value();
            if (divisor == 0) {
                result_ = std::nullopt; // Division by zero
            } else {
                result_ = left_eval.get_value() / divisor;
            }
        } else {
            result_ = std::nullopt;
        }
    }
};

} // namespace planmt
