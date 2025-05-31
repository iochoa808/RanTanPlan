#include "smt_encoding_visitor.h"
#include "../problem/visitors/expression_visitor.h"

namespace planmt {

void SmtEncodingVisitor::visit_symbol(const std::string& symbol, Expression::Kind kind) {
    // For symbols, we need to determine the appropriate type
    // Default to integer for now, but this could be extended based on context
    result_ = create_int_variable(symbol);
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
    auto func_it = symbol_table_.find(function_name);
    
    if (func_it == symbol_table_.end() || !std::holds_alternative<z3::func_decl>(func_it->second)) {
        // Create new function declaration
        // For simplicity, assume all arguments are integers and return type is integer
        std::vector<z3::sort> domain;
        for (size_t i = 0; i < args.size(); ++i) {
            domain.push_back(ctx_.int_sort());
        }
        z3::func_decl func_decl = ctx_.function(function_name.c_str(), 
                                 static_cast<unsigned>(args.size()),
                                 domain.data(), 
                                 ctx_.int_sort());
        symbol_table_.emplace(function_name, func_decl);
        func_it = symbol_table_.find(function_name);
    }
    
    z3::func_decl func_decl = std::get<z3::func_decl>(func_it->second);
    
    // Apply function
    z3::expr_vector z3_args(ctx_);
    for (const auto& arg : args) {
        z3_args.push_back(arg);
    }
    
    return func_decl(z3_args);
}

z3::expr SmtEncodingVisitor::create_bool_variable(const std::string& name) {
    auto it = symbol_table_.find(name);
    if (it != symbol_table_.end()) {
        if (std::holds_alternative<z3::expr>(it->second)) {
            return std::get<z3::expr>(it->second);
        }
    }
    
    z3::expr var = ctx_.bool_const(name.c_str());
    symbol_table_.emplace(name, var);
    return var;
}

z3::expr SmtEncodingVisitor::create_int_variable(const std::string& name) {
    auto it = symbol_table_.find(name);
    if (it != symbol_table_.end()) {
        if (std::holds_alternative<z3::expr>(it->second)) {
            return std::get<z3::expr>(it->second);
        }
    }
    
    z3::expr var = ctx_.int_const(name.c_str());
    symbol_table_.emplace(name, var);
    return var;
}

z3::expr SmtEncodingVisitor::create_real_variable(const std::string& name) {
    auto it = symbol_table_.find(name);
    if (it != symbol_table_.end()) {
        if (std::holds_alternative<z3::expr>(it->second)) {
            return std::get<z3::expr>(it->second);
        }
    }
    
    z3::expr var = ctx_.real_const(name.c_str());
    symbol_table_.emplace(name, var);
    return var;
}

} // namespace planmt
