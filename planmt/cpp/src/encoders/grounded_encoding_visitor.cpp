#include "grounded_encoding_visitor.h"
#include "grounded_encoder.h"
#include "../problem/fluent.h"
#include <iostream>

namespace planmt {

GroundedEncodingVisitor::GroundedEncodingVisitor(z3::context& ctx, 
                                                 GroundedEncoder* encoder,
                                                 const Problem* problem)
    : ctx_(ctx), encoder_(encoder), problem_(problem), current_timestep_(-1) {}

void GroundedEncodingVisitor::visit_symbol(const std::string& symbol, Expression::Kind kind, Expression::Type type) {
    // For symbols, we create a variable or constant based on the type
    switch (type) {
        case Expression::Type::BOOLEAN:
            result_ = ctx_.bool_const(symbol.c_str());
            break;
        case Expression::Type::INTEGER:
            result_ = ctx_.int_const(symbol.c_str());
            break;
        case Expression::Type::REAL:
            result_ = ctx_.real_const(symbol.c_str());
            break;
        case Expression::Type::OBJECT:
            // Objects are typically mapped to integers in SMT encoding
            result_ = ctx_.int_const(symbol.c_str());
            break;
        case Expression::Type::UNKNOWN:
        default:
            // For unknown types, default to integer
            result_ = ctx_.int_const(symbol.c_str());
            break;
    }
}

void GroundedEncodingVisitor::visit_integer(int64_t value, Expression::Kind kind) {
    result_ = ctx_.int_val(static_cast<int>(value));
}

void GroundedEncodingVisitor::visit_real(const Real& value, Expression::Kind kind) {
    result_ = ctx_.real_val(value.to_string().c_str());
}

void GroundedEncodingVisitor::visit_boolean(bool value, Expression::Kind kind) {
    result_ = ctx_.bool_val(value);
}

void GroundedEncodingVisitor::visit_function_application(const std::string& function_name, 
                                                       const std::vector<Expression>& args,
                                                       Expression::Kind kind) {
    // Convert arguments first
    std::vector<z3::expr> z3_args;
    z3_args.reserve(args.size());
    
    for (const auto& arg : args) {
        clear();
        accept_visitor(arg, *this);
        if (!has_result()) {
            std::cerr << "Error: Failed to convert argument in function application: " << function_name << std::endl;
            return;
        }
        z3_args.push_back(get_expression());
    }

    // Use Expression's static method to convert function name to operator enum
    Expression::Operator op = Expression::string_to_operator(function_name);
    
    // Handle operators using enum-based checking (more robust than string comparison)
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
            std::cerr << "Warning: Operator not yet implemented in grounded encoding: " 
                      << function_name << " (enum: " << static_cast<int>(op) << ")" << std::endl;
            result_.reset();
            break;
        case Expression::Operator::UNKNOWN:
        default:
            std::cerr << "Warning: Unknown function in grounded encoding: " << function_name << std::endl;
            result_.reset();
            break;
    }
}

void GroundedEncodingVisitor::visit_fluent_application(const std::string& fluent_name,
                                                     const std::vector<Expression>& args,
                                                     Expression::Kind kind) {
    if (!encoder_) {
        std::cerr << "Error: No GroundedEncoder available for fluent application" << std::endl;
        return;
    }

    // Find the fluent definition
    const Fluent* fluent_def = nullptr;
    for (const auto& fluent : problem_->fluents()) {
        if (fluent.name() == fluent_name) {
            fluent_def = &fluent;
            break;
        }
    }

    if (!fluent_def) {
        std::cerr << "Error: Fluent definition not found for: " << fluent_name << std::endl;
        return;
    }

    // Convert argument expressions to objects
    std::vector<Object> param_objects;
    param_objects.reserve(args.size());
    
    for (const auto& arg : args) {
        // For grounded encoding, arguments should be constants representing objects
        if (arg.kind() == Expression::Kind::CONSTANT && arg.is_atom()) {
            // Get the symbol name from the atom
            param_objects.emplace_back(arg.value().symbol(), "object"); // Default type
        } else {
            std::cerr << "Error: Non-constant argument in grounded fluent application: " << fluent_name 
                      << " (kind: " << static_cast<int>(arg.kind()) << ")" << std::endl;
            return;
        }
    }

    // Use current timestep if available, otherwise use 0
    int timestep = (current_timestep_ >= 0) ? current_timestep_ : 0;
    
    // Get the grounded variable from the encoder
    result_ = encoder_->get_fluent_var(*fluent_def, param_objects, timestep);
}

void GroundedEncodingVisitor::visit_list(const std::vector<Expression>& elements, Expression::Kind kind) {
    // For grounded encoding, lists are not typically expected
    std::cerr << "Warning: List expression not supported in grounded encoding" << std::endl;
    result_.reset();
}

// Arithmetic and logical operation handlers
std::optional<z3::expr> GroundedEncodingVisitor::handle_and(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(true);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result && args[i];
    }
    return result;
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_or(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(false);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result || args[i];
    }
    return result;
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_not(const std::vector<z3::expr>& args) {
    if (args.size() != 1) {
        std::cerr << "Error: 'not' operation expects exactly 1 argument, got " << args.size() << std::endl;
        return std::nullopt;
    }
    return !args[0];
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_equals(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        std::cerr << "Error: '=' operation expects exactly 2 arguments, got " << args.size() << std::endl;
        return std::nullopt;
    }
    return args[0] == args[1];
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_less_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        std::cerr << "Error: '<' operation expects exactly 2 arguments, got " << args.size() << std::endl;
        return std::nullopt;
    }
    return args[0] < args[1];
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_less_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        std::cerr << "Error: '<=' operation expects exactly 2 arguments, got " << args.size() << std::endl;
        return std::nullopt;
    }
    return args[0] <= args[1];
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_greater_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        std::cerr << "Error: '>' operation expects exactly 2 arguments, got " << args.size() << std::endl;
        return std::nullopt;
    }
    return args[0] > args[1];
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_greater_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        std::cerr << "Error: '>=' operation expects exactly 2 arguments, got " << args.size() << std::endl;
        return std::nullopt;
    }
    return args[0] >= args[1];
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_plus(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(0);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result + args[i];
    }
    return result;
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_minus(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        std::cerr << "Error: '-' operation expects at least 1 argument" << std::endl;
        return std::nullopt;
    }
    
    if (args.size() == 1) {
        // Unary minus
        return -args[0];
    } else if (args.size() == 2) {
        // Binary minus
        return args[0] - args[1];
    } else {
        std::cerr << "Error: '-' operation expects 1 or 2 arguments, got " << args.size() << std::endl;
        return std::nullopt;
    }
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_multiply(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(1);
    }
    
    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result * args[i];
    }
    return result;
}

std::optional<z3::expr> GroundedEncodingVisitor::handle_divide(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        std::cerr << "Error: '/' operation expects exactly 2 arguments, got " << args.size() << std::endl;
        return std::nullopt;
    }
    return args[0] / args[1];
}

} // namespace planmt
