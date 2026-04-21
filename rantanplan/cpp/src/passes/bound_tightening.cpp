#include "bound_tightening.hpp"
#include <cmath>

namespace rantanplan {

// ============================================================================
// Helpers
// ============================================================================

static double extract_constant(ExprID id, const ExprPool& pool) {
    if (!id.valid() || !pool.is_constant(id)) return std::nan("");
    if (pool.payload_is_int(id)) return static_cast<double>(pool.payload_int(id));
    if (pool.payload_is_double(id)) return pool.payload_double(id);
    return std::nan("");
}

// Search precondition AND-tree for `fluent >= delta` or `delta <= fluent`.
static bool precondition_has_ge_guard(ExprID precond_id, ExprID fluent_id,
                                      ExprID delta_id, const ExprPool& pool) {
    if (!precond_id.valid()) return false;
    if (pool.kind(precond_id) != ExprKind::FUNCTION_APPLICATION) return false;

    if (pool.is_and(precond_id)) {
        for (ExprID child : pool.children(precond_id)) {
            if (precondition_has_ge_guard(child, fluent_id, delta_id, pool))
                return true;
        }
        return false;
    }

    if (pool.argument_count(precond_id) != 2) return false;
    ExprID lhs = pool.argument(precond_id, 0);
    ExprID rhs = pool.argument(precond_id, 1);

    if (pool.is_greater_equal(precond_id) || pool.is_greater_than(precond_id)) {
        if (lhs == fluent_id && rhs == delta_id) return true;
    }
    if (pool.is_less_equal(precond_id) || pool.is_less_than(precond_id)) {
        if (lhs == delta_id && rhs == fluent_id) return true;
    }

    return false;
}

static bool is_plus_of(ExprID expr, ExprID fluent_id, ExprID delta_id,
                        const ExprPool& pool) {
    if (pool.kind(expr) != ExprKind::FUNCTION_APPLICATION) return false;
    if (!pool.is_plus(expr)) return false;
    if (pool.argument_count(expr) != 2) return false;
    ExprID a0 = pool.argument(expr, 0);
    ExprID a1 = pool.argument(expr, 1);
    return (a0 == fluent_id && a1 == delta_id) ||
           (a0 == delta_id && a1 == fluent_id);
}

static double is_minus_capacity(ExprID expr, ExprID fluent_id,
                                 const ExprPool& pool) {
    if (pool.kind(expr) != ExprKind::FUNCTION_APPLICATION) return std::nan("");
    if (!pool.is_minus(expr)) return std::nan("");
    if (pool.argument_count(expr) != 2) return std::nan("");
    if (pool.argument(expr, 1) != fluent_id) return std::nan("");
    return extract_constant(pool.argument(expr, 0), pool);
}

// Search precondition AND-tree for f + delta <= C (or equivalent forms).
// Returns C if found, +inf otherwise.
static double find_ub_guard(ExprID precond_id, ExprID fluent_id,
                             ExprID delta_id, const ExprPool& pool) {
    if (!precond_id.valid()) return INFINITY;
    if (pool.kind(precond_id) != ExprKind::FUNCTION_APPLICATION) return INFINITY;

    if (pool.is_and(precond_id)) {
        double best = INFINITY;
        for (ExprID child : pool.children(precond_id)) {
            double c = find_ub_guard(child, fluent_id, delta_id, pool);
            if (c < best) best = c;
        }
        return best;
    }

    if (pool.argument_count(precond_id) != 2) return INFINITY;
    ExprID lhs = pool.argument(precond_id, 0);
    ExprID rhs = pool.argument(precond_id, 1);

    if (pool.is_less_equal(precond_id) || pool.is_less_than(precond_id)) {
        if (is_plus_of(lhs, fluent_id, delta_id, pool)) {
            double c = extract_constant(rhs, pool);
            if (std::isfinite(c)) return c;
        }
        if (lhs == delta_id) {
            double c = is_minus_capacity(rhs, fluent_id, pool);
            if (std::isfinite(c)) return c;
        }
    }

    if (pool.is_greater_equal(precond_id) || pool.is_greater_than(precond_id)) {
        if (is_plus_of(rhs, fluent_id, delta_id, pool)) {
            double c = extract_constant(lhs, pool);
            if (std::isfinite(c)) return c;
        }
        if (rhs == delta_id) {
            double c = is_minus_capacity(lhs, fluent_id, pool);
            if (std::isfinite(c)) return c;
        }
    }

    return INFINITY;
}

// ============================================================================
// Lower bound verification: f >= 0
// ============================================================================

static bool verify_lower_bound(ExprID fluent_id, const Problem& problem) {
    const auto& pool = problem.pool();

    for (const auto& action : problem.actions()) {
        for (const auto& effect : action.effects()) {
            const auto& ee = effect.effect_expression();
            if (ee.fluent_id() != fluent_id) continue;

            if (ee.is_conditional()) return false;

            if (ee.is_increase()) {
                if (ee.is_constant_value()) {
                    ExprID val = ee.value_id();
                    double v = 0.0;
                    if (pool.payload_is_int(val))
                        v = static_cast<double>(pool.payload_int(val));
                    else if (pool.payload_is_double(val))
                        v = pool.payload_double(val);
                    if (v < 0.0) return false;
                }
                continue;
            }

            if (ee.is_decrease()) {
                if (!action.has_precondition()) return false;
                if (!precondition_has_ge_guard(action.precondition_id(),
                                               fluent_id, ee.value_id(), pool))
                    return false;
                continue;
            }

            if (ee.is_assign()) {
                ExprID val = ee.value_id();
                if (pool.is_constant(val)) {
                    double v = extract_constant(val, pool);
                    if (std::isfinite(v) && v >= 0.0) continue;
                }
                return false;
            }

            return false;
        }
    }

    return true;
}

// ============================================================================
// Upper bound verification: f <= C
// ============================================================================

static double verify_upper_bound(ExprID fluent_id, const Problem& problem) {
    const auto& pool = problem.pool();
    double bound = -INFINITY;

    for (const auto& action : problem.actions()) {
        for (const auto& effect : action.effects()) {
            const auto& ee = effect.effect_expression();
            if (ee.fluent_id() != fluent_id) continue;

            if (ee.is_conditional()) return INFINITY;

            if (ee.is_decrease()) {
                continue;
            }

            if (ee.is_increase()) {
                if (!action.has_precondition()) return INFINITY;
                double c = find_ub_guard(action.precondition_id(),
                                          fluent_id, ee.value_id(), pool);
                if (!std::isfinite(c)) return INFINITY;
                if (c > bound) bound = c;
                continue;
            }

            if (ee.is_assign()) {
                double v = extract_constant(ee.value_id(), pool);
                if (!std::isfinite(v)) return INFINITY;
                if (v > bound) bound = v;
                continue;
            }

            return INFINITY;
        }
    }

    if (bound == -INFINITY) return INFINITY;

    // Verify initial value <= bound.
    double init = 0.0;
    for (const auto& assignment : problem.initial_state()) {
        if (assignment.fluent_id() == fluent_id) {
            init = extract_constant(assignment.value_id(), pool);
            if (!std::isfinite(init)) return INFINITY;
            break;
        }
    }
    if (init > bound) return INFINITY;

    return bound;
}

// ============================================================================
// Public API
// ============================================================================

int tighten_bounds_syntactically(
    std::unordered_map<ExprID, Interval>& bounds,
    const Problem& problem) {

    const auto& pool = problem.pool();

    // Build initial-value lookup for the lower-bound initial-value check.
    std::unordered_map<ExprID, double> initial_values;
    for (const auto& assignment : problem.initial_state()) {
        ExprID fid = assignment.fluent_id();
        if (!problem.is_numeric_type(fid)) continue;
        ExprID vid = assignment.value_id();
        if (pool.payload_is_int(vid))
            initial_values[fid] = static_cast<double>(pool.payload_int(vid));
        else if (pool.payload_is_double(vid))
            initial_values[fid] = pool.payload_double(vid);
    }

    int tightened = 0;

    for (auto& [eid, interval] : bounds) {
        if (!problem.is_numeric_type(eid)) continue;

        // Lower bound: -inf -> 0 if syntactically verified.
        if (!std::isfinite(interval.lower)) {
            double init = 0.0;
            auto it = initial_values.find(eid);
            if (it != initial_values.end()) init = it->second;

            if (init >= 0.0 && verify_lower_bound(eid, problem)) {
                interval.lower = 0.0;
                tightened++;
            }
        }

        // Upper bound: +inf -> C if syntactically verified.
        if (!std::isfinite(interval.upper)) {
            double c = verify_upper_bound(eid, problem);
            if (std::isfinite(c)) {
                interval.upper = c;
                tightened++;
            }
        }
    }

    return tightened;
}

} // namespace rantanplan
