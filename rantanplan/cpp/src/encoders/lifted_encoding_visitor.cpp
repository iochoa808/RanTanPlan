#include "lifted_encoding_visitor.hpp"
#include <stdexcept>

namespace rantanplan {

// Helper methods for handling specific operators
z3::expr LiftedEncodingVisitor::handle_and(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(true);
    }

    z3::expr_vector z3_args(ctx_);
    for (const auto& arg : args) {
        z3_args.push_back(arg);
    }
    return z3::mk_and(z3_args);
}

z3::expr LiftedEncodingVisitor::handle_or(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(false);
    }

    z3::expr_vector z3_args(ctx_);
    for (const auto& arg : args) {
        z3_args.push_back(arg);
    }
    return z3::mk_or(z3_args);
}

z3::expr LiftedEncodingVisitor::handle_not(const std::vector<z3::expr>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("'not' operation expects exactly 1 argument, got " + std::to_string(args.size()));
    }
    return !args[0];
}

z3::expr LiftedEncodingVisitor::handle_equals(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'=' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] == args[1];
}

z3::expr LiftedEncodingVisitor::handle_less_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'<' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] < args[1];
}

z3::expr LiftedEncodingVisitor::handle_less_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'<=' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] <= args[1];
}

z3::expr LiftedEncodingVisitor::handle_greater_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'>' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] > args[1];
}

z3::expr LiftedEncodingVisitor::handle_greater_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'>=' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] >= args[1];
}

z3::expr LiftedEncodingVisitor::handle_plus(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(0);
    }

    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result + args[i];
    }
    return result;
}

z3::expr LiftedEncodingVisitor::handle_minus(const std::vector<z3::expr>& args) {
    if (args.size() == 1) {
        return -args[0];
    } else if (args.size() == 2) {
        return args[0] - args[1];
    }
    throw std::runtime_error("'-' operation expects 1 or 2 arguments, got " + std::to_string(args.size()));
}

z3::expr LiftedEncodingVisitor::handle_multiply(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(1);
    }

    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result * args[i];
    }
    return result;
}

z3::expr LiftedEncodingVisitor::handle_divide(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'/' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] / args[1];
}

z3::expr LiftedEncodingVisitor::handle_uninterpreted_function(
    const std::string& function_name,
    const std::vector<z3::expr>& args,
    const z3::sort& return_sort) {

    // Create or get function declaration
    auto func_it = symbol_table_.find(function_name);

    if (func_it == symbol_table_.end() || !std::holds_alternative<z3::func_decl>(func_it->second)) {
        // Create new function declaration with appropriate domain sorts
        std::vector<z3::sort> domain;
        for (const auto& arg : args) {
            domain.push_back(arg.get_sort());
        }
        z3::func_decl func_decl = ctx_.function(function_name.c_str(),
                                 static_cast<unsigned>(args.size()),
                                 domain.data(),
                                 return_sort);
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

// ============================================================================
// ExprID-based conversion: walks ExprNode directly via ExprPool
// ============================================================================

z3::expr LiftedEncodingVisitor::convert_from_pool(ExprID id, int timestep) {
    if (!id.valid()) {
        throw std::invalid_argument("convert_from_pool called with invalid ExprID");
    }

    int saved_timestep = current_timestep_;
    if (timestep >= 0) {
        current_timestep_ = timestep;
    }

    auto result = convert_node_pool(id);

    current_timestep_ = saved_timestep;
    return result;
}

z3::expr LiftedEncodingVisitor::convert_node_pool(ExprID id) {
    const ExprPool& pool = problem_->pool();
    const ExprNode& node = pool.get(id);
    auto kind = static_cast<ExprKind>(node.kind);

    // ---- Leaf nodes (no children) ----
    if (node.children.empty()) {
        if (std::holds_alternative<std::string>(node.payload)) {
            const std::string& symbol = std::get<std::string>(node.payload);
            // Resolve type from type_id
            const Type* type = nullptr;
            if (node.type_id >= 0 && node.type_id < static_cast<int>(problem_->types().size())) {
                type = &problem_->types()[node.type_id];
            }
            // Use type-based variable creation (same as visit_symbol)
            if (!type) {
                return create_int_variable(symbol);
            }
            if (type->is_bool()) return create_bool_variable(symbol);
            if (type->is_int()) return create_int_variable(symbol);
            if (type->is_real()) return create_real_variable(symbol);
            // PDDL objects - use integer
            return create_int_variable(symbol);
        }
        if (std::holds_alternative<int64_t>(node.payload)) {
            return ctx_.int_val(static_cast<int>(std::get<int64_t>(node.payload)));
        }
        if (std::holds_alternative<double>(node.payload)) {
            return ctx_.real_val(std::to_string(std::get<double>(node.payload)).c_str());
        }
        if (std::holds_alternative<bool>(node.payload)) {
            return ctx_.bool_val(std::get<bool>(node.payload));
        }
        throw std::runtime_error("Unhandled payload type in leaf node (ExprID " + std::to_string(id.id) + ")");
    }

    // ---- State variable (fluent application) ----
    if (kind == ExprKind::STATE_VARIABLE) {
        // children[0] = fluent symbol, children[1..] = arguments
        const ExprNode& fluent_sym = pool.get(node.children[0]);
        if (!std::holds_alternative<std::string>(fluent_sym.payload)) {
            throw std::runtime_error("STATE_VARIABLE first child is not a symbol");
        }
        const std::string& fluent_name = std::get<std::string>(fluent_sym.payload);

        // Convert argument children to Z3
        std::vector<z3::expr> z3_args;
        z3_args.reserve(node.children.size() - 1);
        for (size_t i = 1; i < node.children.size(); ++i) {
            z3_args.push_back(convert_node_pool(node.children[i]));
        }

        // Add timestep as final argument if temporal encoding is enabled
        if (current_timestep_ >= 0) {
            z3_args.push_back(ctx_.int_val(current_timestep_));
        }

        // Determine return type based on fluent definition
        z3::sort return_sort = ctx_.int_sort();
        if (problem_) {
            const Fluent* fluent_def = problem_->find_fluent(fluent_name);
            if (fluent_def && fluent_def->is_predicate()) {
                return_sort = ctx_.bool_sort();
            }
        }

        return handle_uninterpreted_function(fluent_name, z3_args, return_sort);
    }

    // ---- Function application ----
    if (kind == ExprKind::FUNCTION_APPLICATION) {
        auto op = static_cast<ExprOperator>(node.op);

        // Recursively convert arguments (children[1..], skipping function symbol)
        std::vector<z3::expr> z3_args;
        z3_args.reserve(node.children.size() - 1);
        for (size_t i = 1; i < node.children.size(); ++i) {
            z3_args.push_back(convert_node_pool(node.children[i]));
        }

        switch (op) {
            case ExprOperator::AND:           return handle_and(z3_args);
            case ExprOperator::OR:            return handle_or(z3_args);
            case ExprOperator::NOT:           return handle_not(z3_args);
            case ExprOperator::EQUALS:        return handle_equals(z3_args);
            case ExprOperator::LESS_THAN:     return handle_less_than(z3_args);
            case ExprOperator::LESS_EQUAL:    return handle_less_equal(z3_args);
            case ExprOperator::GREATER_THAN:  return handle_greater_than(z3_args);
            case ExprOperator::GREATER_EQUAL: return handle_greater_equal(z3_args);
            case ExprOperator::PLUS:          return handle_plus(z3_args);
            case ExprOperator::MINUS:         return handle_minus(z3_args);
            case ExprOperator::MULTIPLY:      return handle_multiply(z3_args);
            case ExprOperator::DIVIDE:        return handle_divide(z3_args);
            default: {
                // Get function name from first child
                const ExprNode& func_sym = pool.get(node.children[0]);
                std::string func_name = std::holds_alternative<std::string>(func_sym.payload)
                    ? std::get<std::string>(func_sym.payload)
                    : "unknown_func";
                return handle_uninterpreted_function(func_name, z3_args, ctx_.int_sort());
            }
        }
    }

    throw std::runtime_error("Unhandled ExprNode kind in convert_node: " + std::to_string(node.kind));
}

} // namespace rantanplan
