#include "bound_tightening.hpp"
#include <cmath>
#include <unordered_set>

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

// ============================================================================
// Conservation Law Discovery
// ============================================================================

std::vector<ConservationConstraint> discover_conservation_laws(
    const Problem& problem) {

    const auto& pool = problem.pool();

    // Step 1: For each action, compute net constant delta per numeric fluent.
    // delta > 0 = net increase, delta < 0 = net decrease.
    // Actions with conditional effects or ASSIGN on numeric fluents are
    // marked as "poisoned" for those fluents (can't participate).
    struct ActionDeltas {
        std::unordered_map<ExprID, double> deltas;
        std::unordered_set<ExprID> poisoned;  // conditional/assign/non-constant
    };

    std::vector<ActionDeltas> action_data;
    action_data.reserve(problem.actions().size());

    // Also track which fluents are ever modified (to skip unmodified).
    std::unordered_set<ExprID> ever_modified;

    for (const auto& action : problem.actions()) {
        ActionDeltas ad;
        for (const auto& effect : action.effects()) {
            const auto& ee = effect.effect_expression();
            ExprID fid = ee.fluent_id();
            if (!problem.is_numeric_type(fid)) continue;

            if (ee.is_conditional() || ee.is_assign() || !ee.is_constant_value()) {
                ad.poisoned.insert(fid);
                continue;
            }

            double d = extract_constant(ee.value_id(), pool);
            if (!std::isfinite(d)) { ad.poisoned.insert(fid); continue; }

            if (ee.is_increase()) ad.deltas[fid] += d;
            else if (ee.is_decrease()) ad.deltas[fid] -= d;

            ever_modified.insert(fid);
        }
        action_data.push_back(std::move(ad));
    }

    // Step 2: Collect candidate pairs from actions where two fluents have
    // opposite non-zero deltas. Use ordered pair (min, max) to deduplicate.
    struct PairHash {
        size_t operator()(const std::pair<ExprID, ExprID>& p) const {
            return std::hash<int32_t>()(p.first.id) ^
                   (std::hash<int32_t>()(p.second.id) << 16);
        }
    };
    std::unordered_set<std::pair<ExprID, ExprID>, PairHash> candidate_pairs;

    for (const auto& ad : action_data) {
        for (auto it1 = ad.deltas.begin(); it1 != ad.deltas.end(); ++it1) {
            if (ad.poisoned.count(it1->first)) continue;
            if (std::abs(it1->second) < 1e-12) continue;  // skip zero net delta

            for (auto it2 = std::next(it1); it2 != ad.deltas.end(); ++it2) {
                if (ad.poisoned.count(it2->first)) continue;
                if (std::abs(it2->second) < 1e-12) continue;

                if (std::abs(it1->second + it2->second) < 1e-12) {
                    ExprID a = it1->first, b = it2->first;
                    if (b < a) std::swap(a, b);
                    candidate_pairs.insert({a, b});
                }
            }
        }
    }

    // Step 3: Verify each candidate across ALL actions.
    // For (f, g): every action that modifies f or g must have delta_f + delta_g == 0.
    // Neither f nor g may be poisoned in any action.
    std::vector<ConservationConstraint> result;

    for (const auto& [f, g] : candidate_pairs) {
        bool valid = true;
        for (const auto& ad : action_data) {
            if (ad.poisoned.count(f) || ad.poisoned.count(g)) {
                valid = false;
                break;
            }
            double df = 0.0, dg = 0.0;
            auto itf = ad.deltas.find(f);
            auto itg = ad.deltas.find(g);
            if (itf != ad.deltas.end()) df = itf->second;
            if (itg != ad.deltas.end()) dg = itg->second;

            // If neither is modified by this action, skip (sum preserved).
            if (std::abs(df) < 1e-12 && std::abs(dg) < 1e-12) continue;

            if (std::abs(df + dg) > 1e-12) {
                valid = false;
                break;
            }
        }
        if (!valid) continue;

        // Compute C = initial(f) + initial(g).
        double init_f = 0.0, init_g = 0.0;
        for (const auto& assignment : problem.initial_state()) {
            ExprID fid = assignment.fluent_id();
            if (fid == f) {
                init_f = extract_constant(assignment.value_id(), pool);
                if (!std::isfinite(init_f)) { valid = false; break; }
            } else if (fid == g) {
                init_g = extract_constant(assignment.value_id(), pool);
                if (!std::isfinite(init_g)) { valid = false; break; }
            }
        }
        if (!valid) continue;

        ConservationConstraint cc;
        cc.fluent1_id = f;
        cc.fluent2_id = g;
        cc.constant = init_f + init_g;
        cc.label = pool.to_string(f) + " + " + pool.to_string(g) +
                   " == " + std::to_string(cc.constant);
        result.push_back(std::move(cc));
    }

    return result;
}

// ============================================================================
// Bound Derivation from Conservation Laws
// ============================================================================

void derive_bounds_from_conservations(
    std::unordered_map<ExprID, Interval>& bounds,
    const std::vector<ConservationConstraint>& conservations) {

    for (const auto& cons : conservations) {
        auto it1 = bounds.find(cons.fluent1_id);
        auto it2 = bounds.find(cons.fluent2_id);
        if (it1 == bounds.end() || it2 == bounds.end()) continue;

        double C = cons.constant;

        // f + g == C  =>  f <= C - lower(g),  f >= C - upper(g)
        if (std::isfinite(it2->second.lower))
            it1->second.upper = std::min(it1->second.upper, C - it2->second.lower);
        if (std::isfinite(it2->second.upper))
            it1->second.lower = std::max(it1->second.lower, C - it2->second.upper);

        // Symmetric: g <= C - lower(f),  g >= C - upper(f)
        if (std::isfinite(it1->second.lower))
            it2->second.upper = std::min(it2->second.upper, C - it1->second.lower);
        if (std::isfinite(it1->second.upper))
            it2->second.lower = std::max(it2->second.lower, C - it1->second.upper);
    }
}

} // namespace rantanplan
