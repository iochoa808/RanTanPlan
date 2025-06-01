#include "lifted_encoding_visitor.h"
#include "../problem/visitors/expression_visitor.h"

namespace planmt {

void LiftedEncodingVisitor::visit_symbol(const std::string& symbol, Expression::Kind kind, Expression::Type type) {
    switch (type) {
        case Expression::Type::BOOLEAN:
            result_ = create_bool_variable(symbol);
            break;
        case Expression::Type::INTEGER:
            result_ = create_int_variable(symbol);
            break;
        case Expression::Type::REAL:
            result_ = create_real_variable(symbol);
            break;
        case Expression::Type::OBJECT:
            // PDDL objects (aircraft, city, etc.) - use integer for distinctness
            result_ = create_int_variable(symbol);
            break;
        default:
            std::cerr << "Warning: Unknown type for symbol '" << symbol << "'" << std::endl;
            result_ = create_int_variable(symbol);
            break;
    }
}

void LiftedEncodingVisitor::visit_integer(int64_t value, Expression::Kind kind) {
    result_ = ctx_.int_val(static_cast<int>(value));
}

void LiftedEncodingVisitor::visit_real(const Real& value, Expression::Kind kind) {
    // Convert real to Z3 rational
    result_ = ctx_.real_val(value.numerator(), value.denominator());
}

void LiftedEncodingVisitor::visit_boolean(bool value, Expression::Kind kind) {
    result_ = ctx_.bool_val(value);
}

void LiftedEncodingVisitor::visit_function_application(const std::string& function_name, 
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
    
    // Use enum-based operator handling for efficiency and type safety
    std::cout << "Handling function application: " << function_name << std::endl;
    Expression::Operator op = Expression::string_to_operator(function_name);
    
    switch (op) {
        case Expression::Operator::AND:
            result_ = handle_and(z3_args);
            break;
        case Expression::Operator::OR:
            result_ = handle_or(z3_args);
            break;
        case Expression::Operator::NOT:
            result_ = handle_not(z3_args);
            break;
        case Expression::Operator::EQUALS:
            result_ = handle_equals(z3_args);
            break;
        case Expression::Operator::LESS_THAN:
            result_ = handle_less_than(z3_args);
            break;
        case Expression::Operator::LESS_EQUAL:
            result_ = handle_less_equal(z3_args);
            break;
        case Expression::Operator::GREATER_THAN:
            result_ = handle_greater_than(z3_args);
            break;
        case Expression::Operator::GREATER_EQUAL:
            result_ = handle_greater_equal(z3_args);
            break;
        case Expression::Operator::PLUS:
            result_ = handle_plus(z3_args);
            break;
        case Expression::Operator::MINUS:
            result_ = handle_minus(z3_args);
            break;
        case Expression::Operator::MULTIPLY:
            result_ = handle_multiply(z3_args);
            break;
        case Expression::Operator::DIVIDE:
            result_ = handle_divide(z3_args);
            break;
        case Expression::Operator::MODULO:
        case Expression::Operator::ABSOLUTE:
        case Expression::Operator::MAXIMUM:
        case Expression::Operator::MINIMUM:
        case Expression::Operator::IMPLIES:
        case Expression::Operator::IFF:
            // TODO: Implement these operators when needed
            result_ = handle_uninterpreted_function(function_name, z3_args, ctx_.int_sort());
            break;
        case Expression::Operator::UNKNOWN:
        default:
            // Unknown function - create uninterpreted function
            result_ = handle_uninterpreted_function(function_name, z3_args, ctx_.int_sort());
            break;
    }
}

void LiftedEncodingVisitor::visit_fluent_application(const std::string& fluent_name,
                                                const std::vector<Expression>& args,
                                                Expression::Kind kind) {
    // Convert arguments to Z3
    std::vector<z3::expr> z3_args;
    for (const auto& arg : args) {
        accept_visitor(arg, *this);
        if (!result_) {
            return; // Error in argument conversion
        }
        z3_args.push_back(*result_);
        result_.reset();
    }
    
    // Add timestep as final argument if temporal encoding is enabled
    if (current_timestep_ >= 0) {
        z3_args.push_back(ctx_.int_val(current_timestep_));
    }
    
    // Determine return type based on fluent definition
    z3::sort return_sort = ctx_.int_sort(); // Default to integer
    
    if (problem_) {
        const Fluent* fluent_def = problem_->find_fluent(fluent_name);
        if (fluent_def && fluent_def->is_predicate()) {
            return_sort = ctx_.bool_sort();
        }
    }
    
    // Handle as uninterpreted function with correct return type and timestep
    result_ = handle_uninterpreted_function(fluent_name, z3_args, return_sort);
}

void LiftedEncodingVisitor::visit_list(const std::vector<Expression>& elements, 
                                   Expression::Kind kind) {
    // For lists that aren't function applications, we can't easily convert to Z3
    // This might represent a raw list structure
    result_ = std::nullopt;
}

// Helper methods for handling specific operators
std::optional<z3::expr> LiftedEncodingVisitor::handle_and(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(true);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result && args[i];
    }
    return result;
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_or(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(false);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result || args[i];
    }
    return result;
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_not(const std::vector<z3::expr>& args) {
    if (args.size() != 1) {
        return std::nullopt;
    }
    return !args[0];
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_equals(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] == args[1];
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_less_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] < args[1];
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_less_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] <= args[1];
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_greater_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] > args[1];
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_greater_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] >= args[1];
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_plus(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(0);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result + args[i];
    }
    return result;
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_minus(const std::vector<z3::expr>& args) {
    if (args.size() == 1) {
        // Unary minus
        return -args[0];
    } else if (args.size() == 2) {
        // Binary minus
        return args[0] - args[1];
    }
    return std::nullopt;
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_multiply(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(1);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result * args[i];
    }
    return result;
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_divide(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        return std::nullopt;
    }
    return args[0] / args[1];
}

std::optional<z3::expr> LiftedEncodingVisitor::handle_uninterpreted_function(
    const std::string& function_name, 
    const std::vector<z3::expr>& args,
    const z3::sort& return_sort) {
    
    // Create or get function declaration
    auto func_it = symbol_table_.find(function_name);
    
    if (func_it == symbol_table_.end() || !std::holds_alternative<z3::func_decl>(func_it->second)) {
        // Create new function declaration with appropriate domain sorts
        std::vector<z3::sort> domain;
        for (const auto& arg : args) {
            // Use the actual sort of each argument to maintain type consistency
            domain.push_back(arg.get_sort());
        }
        z3::func_decl func_decl = ctx_.function(function_name.c_str(), 
                                 static_cast<unsigned>(args.size()),
                                 domain.data(), 
                                 return_sort);
        symbol_table_.emplace(function_name, func_decl);
        func_it = symbol_table_.find(function_name);
        
        std::cout << "Created function declaration: " << func_decl << std::endl;
    }
    
    z3::func_decl func_decl = std::get<z3::func_decl>(func_it->second);
    
    // Apply function
    z3::expr_vector z3_args(ctx_);
    for (const auto& arg : args) {
        z3_args.push_back(arg);
    }
    
    return func_decl(z3_args);
}

z3::expr LiftedEncodingVisitor::create_bool_variable(const std::string& name) {
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

z3::expr LiftedEncodingVisitor::create_int_variable(const std::string& name) {
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

z3::expr LiftedEncodingVisitor::create_real_variable(const std::string& name) {
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
