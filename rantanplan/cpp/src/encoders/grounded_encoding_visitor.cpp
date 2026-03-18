#include "grounded_encoding_visitor.hpp"
#include "z3_variable_factory.hpp"
#include "../problem/fluent.hpp"
#include "../problem/effect_expression.hpp"
#include <stdexcept>

namespace rantanplan {

GroundedEncodingVisitor::GroundedEncodingVisitor(z3::context& ctx,
                                                 const Problem* problem,
                                                 Z3VariableFactory* factory)
    : ctx_(ctx), problem_(problem), current_timestep_(-1),
      variable_factory_(factory) {}

// Arithmetic and logical operation handlers
z3::expr GroundedEncodingVisitor::handle_and(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(true);
    }

    z3::expr_vector z3_args(ctx_);
    for (const auto& arg : args) {
        z3_args.push_back(arg);
    }
    return z3::mk_and(z3_args);
}

z3::expr GroundedEncodingVisitor::handle_or(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.bool_val(false);
    }

    z3::expr_vector z3_args(ctx_);
    for (const auto& arg : args) {
        z3_args.push_back(arg);
    }
    return z3::mk_or(z3_args);
}

z3::expr GroundedEncodingVisitor::handle_not(const std::vector<z3::expr>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("'not' operation expects exactly 1 argument, got " + std::to_string(args.size()));
    }
    return !args[0];
}

z3::expr GroundedEncodingVisitor::handle_equals(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'=' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] == args[1];
}

z3::expr GroundedEncodingVisitor::handle_less_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'<' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] < args[1];
}

z3::expr GroundedEncodingVisitor::handle_less_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'<=' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] <= args[1];
}

z3::expr GroundedEncodingVisitor::handle_greater_than(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'>' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] > args[1];
}

z3::expr GroundedEncodingVisitor::handle_greater_equal(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'>=' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] >= args[1];
}

z3::expr GroundedEncodingVisitor::handle_plus(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(0);
    }

    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result + args[i];
    }
    return result;
}

z3::expr GroundedEncodingVisitor::handle_minus(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        throw std::runtime_error("'-' operation expects at least 1 argument");
    }

    if (args.size() == 1) {
        return -args[0];
    } else if (args.size() == 2) {
        return args[0] - args[1];
    } else {
        throw std::runtime_error("'-' operation expects 1 or 2 arguments, got " + std::to_string(args.size()));
    }
}

z3::expr GroundedEncodingVisitor::handle_multiply(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(1);
    }

    z3::expr result = args[0];
    for (size_t i = 1; i < args.size(); ++i) {
        result = result * args[i];
    }
    return result;
}

z3::expr GroundedEncodingVisitor::handle_divide(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'/' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return args[0] / args[1];
}

z3::expr GroundedEncodingVisitor::handle_implies(const std::vector<z3::expr>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("'implies' operation expects exactly 2 arguments, got " + std::to_string(args.size()));
    }
    return z3::implies(args[0], args[1]);
}

// ============================================================================
// ExprID-based conversion: walks ExprNode directly via ExprPool
// ============================================================================

z3::expr GroundedEncodingVisitor::convert_from_pool(ExprID id, int timestep) {
    if (!id.valid()) {
        throw std::invalid_argument("convert_from_pool called with invalid ExprID");
    }

    int saved_timestep = current_timestep_;
    if (timestep >= 0) {
        current_timestep_ = timestep;
    }

    auto result = convert_node(id);

    current_timestep_ = saved_timestep;
    return result;
}

z3::expr GroundedEncodingVisitor::convert_node(ExprID id) {
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
            // Object constants must be encoded as concrete numeric values
            // (not free Z3 variables) to preserve distinct-object semantics
            // in equality comparisons like (= (location ?p) city0).
            // We use make_numeric_val to respect the all_integer sort policy:
            // Int sort when all_integer is true, Real sort otherwise.
            // This avoids Int/Real sort mixing that would crash Z3.
            if (type && type->is_object() && problem_) {
                int idx = problem_->find_object_index(symbol);
                if (idx >= 0) {
                    return variable_factory_->make_numeric_val(static_cast<int64_t>(idx));
                }
            }
            return variable_factory_->create_symbol_variable(symbol, type);
        }
        if (std::holds_alternative<int64_t>(node.payload)) {
            return variable_factory_->make_numeric_val(std::get<int64_t>(node.payload));
        }
        if (std::holds_alternative<double>(node.payload)) {
            return variable_factory_->make_numeric_val(std::get<double>(node.payload));
        }
        if (std::holds_alternative<bool>(node.payload)) {
            return ctx_.bool_val(std::get<bool>(node.payload));
        }
        throw std::runtime_error("Unhandled payload type in leaf node (ExprID " + std::to_string(id.id) + ")");
    }

    // ---- State variable (fluent application) ----
    if (kind == ExprKind::STATE_VARIABLE) {
        const ExprNode& fluent_sym = pool.head_symbol_node(id);
        if (!std::holds_alternative<std::string>(fluent_sym.payload)) {
            throw std::runtime_error("STATE_VARIABLE first child is not a symbol");
        }
        const std::string& fluent_name = std::get<std::string>(fluent_sym.payload);

        // Find fluent definition
        const Fluent* fluent_def = nullptr;
        for (const auto& fluent : problem_->fluents()) {
            if (fluent.name() == fluent_name) {
                fluent_def = &fluent;
                break;
            }
        }
        if (!fluent_def) {
            throw std::runtime_error("Fluent definition not found for: " + fluent_name);
        }

        // Build grounded parameters from argument children
        std::vector<Parameter> grounded_params;
        grounded_params.reserve(pool.argument_count(id));
        size_t arg_index = 0;
        for (ExprID arg_id : pool.arguments(id)) {
            const ExprNode& arg = pool.get(arg_id);
            if (!std::holds_alternative<std::string>(arg.payload)) {
                throw std::runtime_error(
                    "Function composition (nested fluent terms) is not supported. "
                    "Fluent '" + fluent_name + "' has a non-constant argument "
                    "(another fluent application). Rewrite the domain using "
                    "auxiliary action parameters or boolean predicates.");
            }
            const Type* param_type = nullptr;
            if (arg_index < fluent_def->parameters().size()) {
                param_type = fluent_def->parameters()[arg_index].type();
            }
            if (!param_type) {
                param_type = problem_->find_type("object");
            }
            grounded_params.emplace_back(std::get<std::string>(arg.payload), param_type);
            ++arg_index;
        }

        Fluent grounded_fluent(fluent_def->name(), fluent_def->value_type(), grounded_params);
        int timestep = (current_timestep_ >= 0) ? current_timestep_ : 0;
        return variable_factory_->get_fluent_variable(grounded_fluent, timestep);
    }

    // ---- Function application ----
    if (kind == ExprKind::FUNCTION_APPLICATION) {
        auto op = static_cast<ExprOperator>(node.op);

        // Recursively convert arguments
        std::vector<z3::expr> z3_args;
        z3_args.reserve(pool.argument_count(id));
        for (ExprID arg : pool.arguments(id)) {
            z3_args.push_back(convert_node(arg));
        }

        // Dispatch to existing handlers
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
            case ExprOperator::IMPLIES:       return handle_implies(z3_args);
            default:
                throw std::runtime_error("Unsupported operator in ExprID conversion: " + std::to_string(static_cast<int>(op)));
        }
    }

    throw std::runtime_error("Unhandled ExprNode kind in convert_node: " + std::to_string(node.kind));
}

} // namespace rantanplan
