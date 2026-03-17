#include "numeric_constraint_analyzer.hpp"

namespace rantanplan {

// ============================================================================
// Arithmetic expression classifier
// ============================================================================

NumericConstraintAnalyzer::TermDescriptor
NumericConstraintAnalyzer::classify_arithmetic_expr(ExprID id, const ExprPool& pool,
                                                     const Problem& problem) {
    if (!id.valid()) return {0, true};

    ExprKind kind = pool.kind(id);

    // Leaf nodes
    if (pool.is_leaf(id)) {
        switch (kind) {
            case ExprKind::CONSTANT:
            case ExprKind::PARAMETER:
            case ExprKind::VARIABLE:
                return {0, true};
            case ExprKind::STATE_VARIABLE:
            case ExprKind::FLUENT_SYMBOL:
                return {1, true};
            default:
                return {0, true};
        }
    }

    // State variable with children (grounded fluent application like (fuel airplane1))
    if (kind == ExprKind::STATE_VARIABLE) {
        return {1, true};
    }

    // Function application — classify based on operator
    if (kind != ExprKind::FUNCTION_APPLICATION) {
        return {0, true};  // conservative: treat unknown as constant
    }

    ExprOperator op = pool.op(id);

    // Classify argument children
    TermDescriptor combined{0, true};
    for (ExprID arg : pool.arguments(id)) {
        auto child = classify_arithmetic_expr(arg, pool, problem);
        combined.var_count += child.var_count;
        combined.is_linear = combined.is_linear && child.is_linear;
    }

    // If all operands are constant, result is constant regardless of operator
    if (combined.var_count == 0) {
        return {0, true};
    }

    switch (op) {
        case ExprOperator::PLUS:
        case ExprOperator::MINUS:
            // Addition/subtraction preserve linearity
            return combined;

        case ExprOperator::MULTIPLY: {
            if (pool.argument_count(id) == 2) {
                auto lhs = classify_arithmetic_expr(pool.argument(id, 0), pool, problem);
                auto rhs = classify_arithmetic_expr(pool.argument(id, 1), pool, problem);
                if (lhs.var_count == 0) return rhs;
                if (rhs.var_count == 0) return lhs;
            }
            return {combined.var_count, false};  // var * var = nonlinear
        }

        case ExprOperator::DIVIDE: {
            if (pool.argument_count(id) == 2) {
                auto divisor = classify_arithmetic_expr(pool.argument(id, 1), pool, problem);
                if (divisor.var_count == 0) {
                    return classify_arithmetic_expr(pool.argument(id, 0), pool, problem);
                }
            }
            return {combined.var_count, false};  // var / var = nonlinear
        }

        case ExprOperator::ABSOLUTE:
        case ExprOperator::MODULO:
        case ExprOperator::MAXIMUM:
        case ExprOperator::MINIMUM:
            return {combined.var_count, false};  // piecewise = nonlinear

        default:
            return combined;
    }
}

// ============================================================================
// Comparison → profile mapping
// ============================================================================

ArithmeticProfile
NumericConstraintAnalyzer::comparison_profile(const TermDescriptor& lhs,
                                               const TermDescriptor& rhs) {
    bool is_linear = lhs.is_linear && rhs.is_linear;
    int total_vars = lhs.var_count + rhs.var_count;

    if (!is_linear) return ArithmeticProfile::NONLINEAR;
    if (total_vars == 0) return ArithmeticProfile::NONE;
    if (total_vars <= 2) return ArithmeticProfile::DIFFERENCE_LOGIC;
    return ArithmeticProfile::LINEAR;
}

// ============================================================================
// Formula walker
// ============================================================================

ArithmeticProfile
NumericConstraintAnalyzer::classify_formula(ExprID id, const ExprPool& pool,
                                             const Problem& problem) {
    if (!id.valid()) return ArithmeticProfile::NONE;

    ExprKind kind = pool.kind(id);

    // Boolean constant
    if (kind == ExprKind::CONSTANT && pool.payload_is_bool(id)) {
        return ArithmeticProfile::NONE;
    }

    // Boolean state variable — no arithmetic
    if (kind == ExprKind::STATE_VARIABLE) {
        return ArithmeticProfile::NONE;
    }

    // Non-function-application leaf — no arithmetic
    if (kind != ExprKind::FUNCTION_APPLICATION) {
        return ArithmeticProfile::NONE;
    }

    ExprOperator op = pool.op(id);

    // Logical connectives — recurse and take max
    if (is_logical_operator(op)) {
        ArithmeticProfile worst = ArithmeticProfile::NONE;
        for (ExprID arg : pool.arguments(id)) {
            ArithmeticProfile child = classify_formula(arg, pool, problem);
            worst = std::max(worst, child);
            if (worst == ArithmeticProfile::NONLINEAR) return worst;  // short-circuit
        }
        return worst;
    }

    // Comparison operators — classify both sides as arithmetic expressions
    if (is_comparison_operator(op)) {
        if (pool.argument_count(id) != 2) return ArithmeticProfile::LINEAR;  // safe fallback

        ExprID lhs_id = pool.argument(id, 0);
        ExprID rhs_id = pool.argument(id, 1);

        // If both sides are boolean, this is a boolean equality — not arithmetic
        if (problem.is_bool_type(lhs_id) && problem.is_bool_type(rhs_id)) {
            return ArithmeticProfile::NONE;
        }

        auto lhs = classify_arithmetic_expr(lhs_id, pool, problem);
        auto rhs = classify_arithmetic_expr(rhs_id, pool, problem);
        return comparison_profile(lhs, rhs);
    }

    // Arithmetic operator at formula level (shouldn't happen, but safe fallback)
    return ArithmeticProfile::LINEAR;
}

// ============================================================================
// Main analysis entry point
// ============================================================================

ConstraintAnalysisResult NumericConstraintAnalyzer::analyze(const Problem& problem) {
    ConstraintAnalysisResult result;
    const ExprPool& pool = problem.pool();

    // Count numeric fluents
    for (const auto& fluent : problem.fluents()) {
        if (fluent.is_int_fluent() || fluent.is_real_fluent()) {
            result.num_numeric_fluents++;
        }
    }

    // Fast path: no numeric fluents → purely propositional
    if (result.num_numeric_fluents == 0) {
        result.profile = ArithmeticProfile::NONE;
        return result;
    }

    ArithmeticProfile worst = ArithmeticProfile::NONE;

    auto update = [&](ArithmeticProfile p) {
        worst = std::max(worst, p);
    };

    // Classify action preconditions, effects, and costs
    for (const auto& action : problem.actions()) {
        if (action.has_precondition()) {
            update(classify_formula(action.precondition_id(), pool, problem));
            if (worst == ArithmeticProfile::NONLINEAR) break;
        }

        for (const auto& effect : action.effects()) {
            const auto& eff_expr = effect.effect_expression();

            // Use existing ValueKind classification for effect values
            switch (eff_expr.value_kind()) {
                case ValueKind::CONSTANT:
                    update(ArithmeticProfile::DIFFERENCE_LOGIC);
                    break;
                case ValueKind::LINEAR:
                    update(ArithmeticProfile::LINEAR);
                    break;
                case ValueKind::NONLINEAR:
                    update(ArithmeticProfile::NONLINEAR);
                    break;
            }

            // Also classify conditional effect conditions
            if (eff_expr.is_conditional()) {
                update(classify_formula(eff_expr.condition_id(), pool, problem));
            }

            if (worst == ArithmeticProfile::NONLINEAR) break;
        }
        if (worst == ArithmeticProfile::NONLINEAR) break;

        // Classify action cost expression
        if (action.has_explicit_cost()) {
            auto cost_desc = classify_arithmetic_expr(action.cost_id(), pool, problem);
            if (!cost_desc.is_linear) {
                update(ArithmeticProfile::NONLINEAR);
            } else if (cost_desc.var_count > 0) {
                update(ArithmeticProfile::LINEAR);
            }
        }
        if (worst == ArithmeticProfile::NONLINEAR) break;
    }

    // Classify goals
    if (worst < ArithmeticProfile::NONLINEAR) {
        for (const auto& goal : problem.goals()) {
            update(classify_formula(goal.goal_id(), pool, problem));
            if (worst == ArithmeticProfile::NONLINEAR) break;
        }
    }

    result.profile = worst;
    return result;
}

} // namespace rantanplan
