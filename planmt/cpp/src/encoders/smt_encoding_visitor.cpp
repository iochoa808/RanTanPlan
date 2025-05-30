#include "smt_encoding_visitor.h"
#include "../problem/visitors/expression_visitor.h"

namespace planmt {

void SmtEncodingVisitor::visit_symbol(const std::string& symbol, Expression::Kind kind) {
    result_ = get_or_create_variable(symbol, kind);
}

void SmtEncodingVisitor::visit_integer(int64_t value, Expression::Kind kind) {
    result_ = ctx_.int_val(static_cast<int>(value));
}

void SmtEncodingVisitor::visit_real(const Real& value, Expression::Kind kind) {
    // Convert real to Z3 rational
    result_ = ctx_.real_val(value.numerator(), value.denominator());
}

void SmtEncodingVisitor::visit_boolean(bool value, Expression::Kind kind) {
    result_ = ctx_.bool_val(value);
}

void SmtEncodingVisitor::visit_function_application(const std::string& function_name, 
                                                  const std::vector<Expression>& args,
                                                  Expression::Kind kind) {
    // Convert arguments
    std::vector<z3::expr> z3_args;
    for (const auto& arg : args) {
        accept_visitor(arg, *this);
        if (result_) {
            z3_args.push_back(*result_);
        } else {
            result_ = std::nullopt;
            return;
        }
    }
    
    // Handle different operators
    if (function_name == "and") {
        result_ = handle_and(z3_args);
    } else if (function_name == "or") {
        result_ = handle_or(z3_args);
    } else if (function_name == "not") {
        result_ = handle_not(z3_args);
    } else if (function_name == "=" || function_name == "==") {
        result_ = handle_equals(z3_args);
    } else if (function_name == "<") {
        result_ = handle_less_than(z3_args);
    } else if (function_name == "<=") {
        result_ = handle_less_equal(z3_args);
    } else if (function_name == ">") {
        result_ = handle_greater_than(z3_args);
    } else if (function_name == ">=") {
        result_ = handle_greater_equal(z3_args);
    } else if (function_name == "+") {
        result_ = handle_plus(z3_args);
    } else if (function_name == "-") {
        result_ = handle_minus(z3_args);
    } else if (function_name == "*") {
        result_ = handle_multiply(z3_args);
    } else if (function_name == "/") {
        result_ = handle_divide(z3_args);
    } else {
        // Unknown function - create uninterpreted function
        result_ = handle_uninterpreted_function(function_name, z3_args);
    }
}

void SmtEncodingVisitor::visit_fluent_application(const std::string& fluent_name,
                                                const std::vector<Expression>& args,
                                                Expression::Kind kind) {
    // Fluent applications are similar to function applications
    visit_function_application(fluent_name, args, kind);
}

void SmtEncodingVisitor::visit_list(const std::vector<Expression>& elements, 
                                   Expression::Kind kind) {
    // For lists that aren't function applications, we can't easily convert to Z3
    // This might represent a raw list structure
    result_ = std::nullopt;
}

// Helper methods for handling specific operators
std::optional<z3::expr> SmtEncodingVisitor::handle_and(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(true);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result && args[i];
    }
    return result;
}

std::optional<z3::expr> SmtEncodingVisitor::handle_or(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(false);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result || args[i];
    }
    return result;
}

std::optional<z3::expr> SmtEncodingVisitor::handle_not(const std::vector<z3::expr>& args) {
    if (args.size() != 1) {
        return std::nullopt;
    }
    return !args[0];
}

std::optional<z3::expr> SmtEncodingVisitor::handle_equals(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] == args[1];
}

std::optional<z3::expr> SmtEncodingVisitor::handle_less_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] < args[1];
}

std::optional<z3::expr> SmtEncodingVisitor::handle_less_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] <= args[1];
}

std::optional<z3::expr> SmtEncodingVisitor::handle_greater_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] > args[1];
}

std::optional<z3::expr> SmtEncodingVisitor::handle_greater_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] >= args[1];
}

std::optional<z3::expr> SmtEncodingVisitor::handle_plus(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(0);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result + args[i];
    }
    return result;
}

std::optional<z3::expr> SmtEncodingVisitor::handle_minus(const std::vector<z3::expr>& args) {
    if (args.size() == 1) {
        // Unary minus
        return -args[0];
    } else if (args.size() == 2) {
        // Binary minus
        return args[0] - args[1];
    }
    return std::nullopt;
}

std::optional<z3::expr> SmtEncodingVisitor::handle_multiply(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(1);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result * args[i];
    }
    return result;
}

std::optional<z3::expr> SmtEncodingVisitor::handle_divide(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] / args[1];
}

std::optional<z3::expr> SmtEncodingVisitor::handle_uninterpreted_function(
    const std::string& function_name, 
    const std::vector<z3::expr>& args) {
    
    // Create or get function declaration
    auto func_it = functions_.find(function_name);
    if (func_it == functions_.end()) {
        // Create new function declaration
        // For simplicity, assume all arguments are integers and return type is integer
        std::vector<z3::sort> domain;
        for (size_t i = 0; i < args.size(); ++i) {
            domain.push_back(ctx_.int_sort());
        }
        z3::func_decl func = ctx_.function(function_name.c_str(), static_cast<unsigned>(args.size()), domain.data(), ctx_.int_sort());
        functions_[function_name] = std::make_shared<z3::func_decl>(func);
        func_it = functions_.find(function_name);
    }
    
    // Apply function
    z3::expr_vector z3_args(ctx_);
    for (const auto& arg : args) {
        z3_args.push_back(arg);
    }
    
    return func_it->second->operator()(z3_args);
}

z3::expr SmtEncodingVisitor::get_or_create_variable(const std::string& name, Expression::Kind kind) {
    auto var_it = variables_.find(name);
    if (var_it != variables_.end()) {
        return *var_it->second;
    }
    
    // Create new variable based on kind
    z3::expr var = ctx_.int_const(name.c_str());  // Default to integer
    
    // Could extend this to handle different types based on kind or type information
    switch (kind) {
        case Expression::Kind::CONSTANT:
        case Expression::Kind::PARAMETER:
        case Expression::Kind::VARIABLE:
        case Expression::Kind::FUNCTION_SYMBOL:
        case Expression::Kind::FLUENT_SYMBOL:
            // For now, all variables are integers
            var = ctx_.int_const(name.c_str());
            break;
        default:
            var = ctx_.int_const(name.c_str());
            break;
    }
    
    variables_[name] = std::make_shared<z3::expr>(var);
    return var;
}

} // namespace planmt
