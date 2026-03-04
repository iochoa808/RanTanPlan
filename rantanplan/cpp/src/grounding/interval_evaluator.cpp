#include "interval_evaluator.hpp"

namespace rantanplan {

Interval evaluate_interval(ExprID eid,
                           const ExprPool& pool,
                           const NumericBoundsIndex& bounds,
                           const Problem& problem) {
    if (!eid.valid()) return Interval::unbounded();

    // --- Leaf: numeric constant ---
    if (pool.is_constant(eid)) {
        if (pool.payload_is_double(eid)) {
            return Interval(pool.payload_double(eid));
        }
        if (pool.payload_is_int(eid)) {
            return Interval(static_cast<double>(pool.payload_int(eid)));
        }
        // Boolean or string constant — not a numeric value.
        return Interval::unbounded();
    }

    // --- Leaf: ground numeric state variable ---
    if (pool.is_state_variable(eid)) {
        int schema_id = -1;
        std::vector<int> obj_indices;
        if (bounds.decompose_numeric_state_variable(eid, schema_id, obj_indices)) {
            return bounds.get_bounds(schema_id, obj_indices);
        }
        // Boolean state variable or decomposition failed.
        return Interval::unbounded();
    }

    // --- Function application: arithmetic operators ---
    if (pool.is_function_application(eid)) {
        ExprOperator op = pool.op(eid);
        size_t nargs = pool.argument_count(eid);

        if (nargs == 2) {
            Interval lhs = evaluate_interval(pool.argument(eid, 0), pool, bounds, problem);
            Interval rhs = evaluate_interval(pool.argument(eid, 1), pool, bounds, problem);

            switch (op) {
                case ExprOperator::PLUS:     return lhs + rhs;
                case ExprOperator::MINUS:    return lhs - rhs;
                case ExprOperator::MULTIPLY: return lhs * rhs;
                case ExprOperator::DIVIDE:   return lhs / rhs;
                default: break;
            }
        }

        // Unary minus: MINUS with 1 argument → negate.
        if (nargs == 1 && op == ExprOperator::MINUS) {
            Interval arg = evaluate_interval(pool.argument(eid, 0), pool, bounds, problem);
            return Interval(0.0) - arg;
        }

        // Unsupported operation (MODULO, ABS, MAX, MIN, etc.).
        return Interval::unbounded();
    }

    // Anything else (PARAMETER, VARIABLE, etc.) — safe fallback.
    return Interval::unbounded();
}

// ---------------------------------------------------------------------------

/// Check if a comparison between two intervals can possibly hold.
static bool comparison_satisfiable(ExprOperator op,
                                    const Interval& lhs,
                                    const Interval& rhs) {
    switch (op) {
        case ExprOperator::LESS_EQUAL:
            // lhs <= rhs is satisfiable if lhs.lower <= rhs.upper.
            return lhs.lower <= rhs.upper;

        case ExprOperator::LESS_THAN:
            // lhs < rhs is satisfiable if lhs.lower < rhs.upper.
            return lhs.lower < rhs.upper;

        case ExprOperator::GREATER_EQUAL:
            // lhs >= rhs is satisfiable if lhs.upper >= rhs.lower.
            return lhs.upper >= rhs.lower;

        case ExprOperator::GREATER_THAN:
            // lhs > rhs is satisfiable if lhs.upper > rhs.lower.
            return lhs.upper > rhs.lower;

        case ExprOperator::EQUALS:
            // lhs == rhs is satisfiable if the intervals overlap.
            return lhs.overlaps(rhs);

        default:
            // Unknown comparison — assume satisfiable.
            return true;
    }
}

bool numeric_precondition_satisfiable(ExprID eid,
                                       const ExprPool& pool,
                                       const NumericBoundsIndex& bounds,
                                       const Problem& problem) {
    if (!eid.valid()) return true;

    // --- AND: all conjuncts must be satisfiable ---
    if (pool.is_and(eid)) {
        if (pool.has_head_and_arguments(eid)) {
            for (ExprID arg : pool.arguments(eid)) {
                if (!numeric_precondition_satisfiable(arg, pool, bounds, problem))
                    return false;
            }
        } else {
            for (ExprID child : pool.children(eid)) {
                if (!numeric_precondition_satisfiable(child, pool, bounds, problem))
                    return false;
            }
        }
        return true;
    }

    // --- OR: at least one disjunct must be satisfiable ---
    if (pool.is_or(eid)) {
        if (pool.has_head_and_arguments(eid)) {
            for (ExprID arg : pool.arguments(eid)) {
                if (numeric_precondition_satisfiable(arg, pool, bounds, problem))
                    return true;
            }
            return false;
        } else {
            for (ExprID child : pool.children(eid)) {
                if (numeric_precondition_satisfiable(child, pool, bounds, problem))
                    return true;
            }
            return false;
        }
    }

    // --- NOT: can't negate interval checks safely → assume satisfiable ---
    if (pool.is_not(eid)) {
        return true;
    }

    // --- Comparison operators (the core numeric check) ---
    if (pool.is_function_application(eid)) {
        ExprOperator op = pool.op(eid);

        if (op == ExprOperator::LESS_EQUAL  || op == ExprOperator::LESS_THAN ||
            op == ExprOperator::GREATER_EQUAL || op == ExprOperator::GREATER_THAN ||
            op == ExprOperator::EQUALS) {

            if (pool.argument_count(eid) == 2) {
                Interval lhs = evaluate_interval(pool.argument(eid, 0), pool, bounds, problem);
                Interval rhs = evaluate_interval(pool.argument(eid, 1), pool, bounds, problem);
                return comparison_satisfiable(op, lhs, rhs);
            }
        }
    }

    // Boolean state variable, IMPLIES, IFF, or anything else — assume satisfiable.
    return true;
}

} // namespace rantanplan
