#include "smt_encoding_visitor.h"

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
    // Handle common arithmetic and logical operations
    if (function_name == "+") {
        encode_addition(args);
    } else if (function_name == "-") {
        encode_subtraction(args);
    } else if (function_name == "*") {
        encode_multiplication(args);
    } else if (function_name == "/") {
        encode_division(args);
    } else if (function_name == "and") {
        encode_and(args);
    } else if (function_name == "or") {
        encode_or(args);
    } else if (function_name == "not") {
        encode_not(args);
    } else if (function_name == "=>" || function_name == "implies") {
        encode_implies(args);
    } else if (function_name == "=" || function_name == "==") {
        encode_equals(args);
    } else if (function_name == "<") {
        encode_less_than(args);
    } else if (function_name == "<=") {
        encode_less_equal(args);
    } else if (function_name == ">") {
        encode_greater_than(args);
    } else if (function_name == ">=") {
        encode_greater_equal(args);
    } else {
        // Unknown function - create uninterpreted function
        // For simplicity, we'll treat it as a boolean function for now
        z3::func_decl func = ctx_.function(function_name.c_str(), ctx_.int_sort(), ctx_.bool_sort());
        if (args.empty()) {
            result_ = func();
        } else {        // Convert first argument and apply function
        SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
        accept_visitor(args[0], arg_visitor);
        if (arg_visitor.has_result()) {
            result_ = func(arg_visitor.get_expression());
        } else {
            result_ = std::nullopt;
        }
        }
    }
}

void SmtEncodingVisitor::visit_fluent_application(const std::string& fluent_name,
                                                const std::vector<Expression>& args,
                                                Expression::Kind kind) {
    // Treat fluent applications as uninterpreted predicates
    // For now, assume they return boolean values
    if (args.empty()) {
        // Zero-arity fluent
        result_ = get_or_create_variable(fluent_name, Expression::Kind::FLUENT_SYMBOL);
    } else {
        // Create uninterpreted predicate
        z3::func_decl fluent = ctx_.function(fluent_name.c_str(), ctx_.int_sort(), ctx_.bool_sort());
        
        // Convert first argument
        SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
        accept_visitor(args[0], arg_visitor);
        if (arg_visitor.has_result()) {
            result_ = fluent(arg_visitor.get_expression());
        } else {
            result_ = std::nullopt;
        }
    }
}

void SmtEncodingVisitor::visit_list(const std::vector<Expression>& elements, 
                                   Expression::Kind kind) {
    // For generic lists, we can't make assumptions about their meaning
    // This should typically not be called for well-formed expressions
    result_ = std::nullopt;
}

void SmtEncodingVisitor::encode_addition(const std::vector<Expression>& args) {
    if (args.empty()) {
        result_ = ctx_.int_val(0); // Additive identity
        return;
    }
    
    // Convert first argument
    SmtEncodingVisitor first_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], first_visitor);
    if (!first_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    z3::expr sum = first_visitor.get_expression();
    
    // Add remaining arguments
    for (size_t i = 1; i < args.size(); ++i) {
        SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
        accept_visitor(args[i], arg_visitor);
        if (!arg_visitor.has_result()) {
            result_ = std::nullopt;
            return;
        }
        sum = sum + arg_visitor.get_expression();
    }
    
    result_ = sum;
}

void SmtEncodingVisitor::encode_subtraction(const std::vector<Expression>& args) {
    if (args.empty()) {
        result_ = std::nullopt;
        return;
    }
    
    // Unary minus
    if (args.size() == 1) {
        SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
        accept_visitor(args[0], arg_visitor);
        if (arg_visitor.has_result()) {
            result_ = -arg_visitor.get_expression();
        } else {
            result_ = std::nullopt;
        }
        return;
    }
    
    // Binary subtraction (left-associative for multiple args)
    SmtEncodingVisitor first_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], first_visitor);
    if (!first_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    z3::expr diff = first_visitor.get_expression();
    
    for (size_t i = 1; i < args.size(); ++i) {
        SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
        accept_visitor(args[i], arg_visitor);
        if (!arg_visitor.has_result()) {
            result_ = std::nullopt;
            return;
        }
        diff = diff - arg_visitor.get_expression();
    }
    
    result_ = diff;
}

void SmtEncodingVisitor::encode_multiplication(const std::vector<Expression>& args) {
    if (args.empty()) {
        result_ = ctx_.int_val(1); // Multiplicative identity
        return;
    }
    
    SmtEncodingVisitor first_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], first_visitor);
    if (!first_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    z3::expr product = first_visitor.get_expression();
    
    for (size_t i = 1; i < args.size(); ++i) {
        SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
        accept_visitor(args[i], arg_visitor);
        if (!arg_visitor.has_result()) {
            result_ = std::nullopt;
            return;
        }
        product = product * arg_visitor.get_expression();
    }
    
    result_ = product;
}

void SmtEncodingVisitor::encode_division(const std::vector<Expression>& args) {
    if (args.size() != 2) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor left_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], left_visitor);
    if (!left_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor right_visitor(ctx_, symbol_cache_);
    accept_visitor(args[1], right_visitor);
    if (!right_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    result_ = left_visitor.get_expression() / right_visitor.get_expression();
}

void SmtEncodingVisitor::encode_and(const std::vector<Expression>& args) {
    if (args.empty()) {
        result_ = ctx_.bool_val(true); // Logical identity for AND
        return;
    }
    
    SmtEncodingVisitor first_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], first_visitor);
    if (!first_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    z3::expr conjunction = first_visitor.get_expression();
    
    for (size_t i = 1; i < args.size(); ++i) {
        SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
        accept_visitor(args[i], arg_visitor);
        if (!arg_visitor.has_result()) {
            result_ = std::nullopt;
            return;
        }
        conjunction = conjunction && arg_visitor.get_expression();
    }
    
    result_ = conjunction;
}

void SmtEncodingVisitor::encode_or(const std::vector<Expression>& args) {
    if (args.empty()) {
        result_ = ctx_.bool_val(false); // Logical identity for OR
        return;
    }
    
    SmtEncodingVisitor first_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], first_visitor);
    if (!first_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    z3::expr disjunction = first_visitor.get_expression();
    
    for (size_t i = 1; i < args.size(); ++i) {
        SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
        accept_visitor(args[i], arg_visitor);
        if (!arg_visitor.has_result()) {
            result_ = std::nullopt;
            return;
        }
        disjunction = disjunction || arg_visitor.get_expression();
    }
    
    result_ = disjunction;
}

void SmtEncodingVisitor::encode_not(const std::vector<Expression>& args) {
    if (args.size() != 1) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor arg_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], arg_visitor);
    if (arg_visitor.has_result()) {
        result_ = !arg_visitor.get_expression();
    } else {
        result_ = std::nullopt;
    }
}

void SmtEncodingVisitor::encode_implies(const std::vector<Expression>& args) {
    if (args.size() != 2) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor left_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], left_visitor);
    if (!left_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor right_visitor(ctx_, symbol_cache_);
    accept_visitor(args[1], right_visitor);
    if (!right_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    result_ = z3::implies(left_visitor.get_expression(), right_visitor.get_expression());
}

void SmtEncodingVisitor::encode_equals(const std::vector<Expression>& args) {
    if (args.size() != 2) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor left_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], left_visitor);
    if (!left_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor right_visitor(ctx_, symbol_cache_);
    accept_visitor(args[1], right_visitor);
    if (!right_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    result_ = left_visitor.get_expression() == right_visitor.get_expression();
}

void SmtEncodingVisitor::encode_less_than(const std::vector<Expression>& args) {
    if (args.size() != 2) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor left_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], left_visitor);
    if (!left_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor right_visitor(ctx_, symbol_cache_);
    accept_visitor(args[1], right_visitor);
    if (!right_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    result_ = left_visitor.get_expression() < right_visitor.get_expression();
}

void SmtEncodingVisitor::encode_less_equal(const std::vector<Expression>& args) {
    if (args.size() != 2) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor left_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], left_visitor);
    if (!left_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor right_visitor(ctx_, symbol_cache_);
    accept_visitor(args[1], right_visitor);
    if (!right_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    result_ = left_visitor.get_expression() <= right_visitor.get_expression();
}

void SmtEncodingVisitor::encode_greater_than(const std::vector<Expression>& args) {
    if (args.size() != 2) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor left_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], left_visitor);
    if (!left_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor right_visitor(ctx_, symbol_cache_);
    accept_visitor(args[1], right_visitor);
    if (!right_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    result_ = left_visitor.get_expression() > right_visitor.get_expression();
}

void SmtEncodingVisitor::encode_greater_equal(const std::vector<Expression>& args) {
    if (args.size() != 2) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor left_visitor(ctx_, symbol_cache_);
    accept_visitor(args[0], left_visitor);
    if (!left_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    SmtEncodingVisitor right_visitor(ctx_, symbol_cache_);
    accept_visitor(args[1], right_visitor);
    if (!right_visitor.has_result()) {
        result_ = std::nullopt;
        return;
    }
    
    result_ = left_visitor.get_expression() >= right_visitor.get_expression();
}

z3::expr SmtEncodingVisitor::get_or_create_variable(const std::string& symbol, Expression::Kind kind) {
    // Check if we already have this symbol cached
    auto it = symbol_cache_.find(symbol);
    if (it != symbol_cache_.end()) {
        return *(it->second);
    }
    
    // Create new Z3 variable based on the kind and symbol name
    z3::sort sort = get_sort_for_symbol(symbol, kind);
    z3::expr var = ctx_.constant(symbol.c_str(), sort);
    
    // Cache for future use
    symbol_cache_[symbol] = std::make_shared<z3::expr>(var);
    return var;
}

z3::sort SmtEncodingVisitor::get_sort_for_symbol(const std::string& symbol, Expression::Kind kind) {
    // Determine sort based on expression kind and symbol naming conventions
    switch (kind) {
        case Expression::Kind::VARIABLE:
        case Expression::Kind::PARAMETER:
        case Expression::Kind::CONSTANT:
            // For variables/parameters, try to infer type from name or default to int
            // This is a simple heuristic - in practice you might want more sophisticated type inference
            if (symbol.find("bool") != std::string::npos || 
                symbol.find("flag") != std::string::npos ||
                symbol.find("_b") != std::string::npos) {
                return ctx_.bool_sort();
            } else if (symbol.find("real") != std::string::npos || 
                       symbol.find("_r") != std::string::npos) {
                return ctx_.real_sort();
            } else {
                return ctx_.int_sort(); // Default to integer
            }
            
        case Expression::Kind::FLUENT_SYMBOL:
        case Expression::Kind::STATE_VARIABLE:
            // Fluents are typically boolean predicates
            return ctx_.bool_sort();
            
        case Expression::Kind::FUNCTION_SYMBOL:
        case Expression::Kind::FUNCTION_APPLICATION:
            // Functions could return any type - default to int
            return ctx_.int_sort();
            
        default:
            // Unknown kind - default to int
            return ctx_.int_sort();
    }
}

} // namespace planmt
