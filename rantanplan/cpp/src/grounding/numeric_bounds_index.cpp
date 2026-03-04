#include "numeric_bounds_index.hpp"
#include "../util/logger.hpp"

namespace rantanplan {

// ---------------------------------------------------------------------------
// Lifted expression evaluator for freeze analysis.
// Like evaluate_interval() but uses per-schema ranges of constant fluents
// instead of ground bounds.  Returns unbounded if any non-constant fluent
// is referenced (signalling "can't determine range statically").
// ---------------------------------------------------------------------------

/// Extract the fluent schema ID from a (possibly lifted) STATE_VARIABLE.
/// Returns -1 if extraction fails.
static int extract_fluent_schema_id(ExprID eid,
                                      const ExprPool& pool,
                                      const Problem& problem) {
    if (!pool.is_state_variable(eid)) return -1;
    ExprID head = pool.head_symbol_id(eid);
    if (!pool.is_fluent_symbol(head)) return -1;
    const std::string& fname = pool.payload_string(head);
    const Fluent* fluent = problem.find_fluent(fname);
    if (!fluent || fluent->is_predicate()) return -1;
    return fluent->id();
}

/// Evaluate a (possibly lifted) expression to an interval using only the
/// ranges of constant fluent schemas.  Non-constant state variables or
/// unsupported nodes yield unbounded (safe fallback).
static Interval evaluate_constant_expr_range(
        ExprID eid,
        const ExprPool& pool,
        const Problem& problem,
        const std::unordered_map<int, Interval>& constant_ranges) {
    if (!eid.valid()) return Interval::unbounded();

    if (pool.is_constant(eid)) {
        if (pool.payload_is_double(eid))
            return Interval(pool.payload_double(eid));
        if (pool.payload_is_int(eid))
            return Interval(static_cast<double>(pool.payload_int(eid)));
        return Interval::unbounded();
    }

    if (pool.is_state_variable(eid)) {
        int sid = extract_fluent_schema_id(eid, pool, problem);
        if (sid < 0) return Interval::unbounded();
        auto it = constant_ranges.find(sid);
        if (it != constant_ranges.end()) return it->second;
        return Interval::unbounded();  // Non-constant fluent.
    }

    if (pool.is_function_application(eid)) {
        ExprOperator op = pool.op(eid);
        size_t nargs = pool.argument_count(eid);

        if (nargs == 2) {
            Interval lhs = evaluate_constant_expr_range(
                pool.argument(eid, 0), pool, problem, constant_ranges);
            Interval rhs = evaluate_constant_expr_range(
                pool.argument(eid, 1), pool, problem, constant_ranges);
            switch (op) {
                case ExprOperator::PLUS:     return lhs + rhs;
                case ExprOperator::MINUS:    return lhs - rhs;
                case ExprOperator::MULTIPLY: return lhs * rhs;
                case ExprOperator::DIVIDE:   return lhs / rhs;
                default: break;
            }
        }
        if (nargs == 1 && op == ExprOperator::MINUS) {
            Interval arg = evaluate_constant_expr_range(
                pool.argument(eid, 0), pool, problem, constant_ranges);
            return Interval(0.0) - arg;
        }
    }

    return Interval::unbounded();
}

NumericBoundsIndex::NumericBoundsIndex(const Problem& problem)
    : problem_(problem) {}

void NumericBoundsIndex::initialize_from_initial_state() {
    const auto& pool = problem_.pool();

    for (const auto& assignment : problem_.initial_state()) {
        ExprID fluent_eid = assignment.fluent_id();
        ExprID value_eid  = assignment.value_id();

        // Extract numeric value from the CONSTANT payload.
        double val = 0.0;
        if (pool.payload_is_double(value_eid)) {
            val = pool.payload_double(value_eid);
        } else if (pool.payload_is_int(value_eid)) {
            val = static_cast<double>(pool.payload_int(value_eid));
        } else {
            // Not a numeric assignment (boolean or string) — skip.
            continue;
        }

        int schema_id = -1;
        std::vector<int> obj_indices;
        if (decompose_numeric_state_variable(fluent_eid, schema_id, obj_indices)) {
            GroundKey key{schema_id, std::move(obj_indices)};
            bounds_.insert_or_assign(key, Interval(val));
            // No expansion count increment — this is the initial state.
        }
    }
}

Interval NumericBoundsIndex::get_bounds(int fluent_schema_id,
                                         const std::vector<int>& obj_indices) const {
    GroundKey key{fluent_schema_id, obj_indices};
    auto it = bounds_.find(key);
    if (it != bounds_.end()) {
        return it->second;
    }
    // Uninitialized numeric fluent defaults to [0, 0] per UP convention.
    return Interval(0.0);
}

bool NumericBoundsIndex::update_bounds(int fluent_schema_id,
                                        const std::vector<int>& obj_indices,
                                        const Interval& new_interval) {
    GroundKey key{fluent_schema_id, obj_indices};
    auto it = bounds_.find(key);

    if (it == bounds_.end()) {
        // First time seeing this fluent. Default is [0, 0].
        Interval def(0.0);
        Interval merged = def.convex_union(new_interval);
        bounds_.emplace(key, merged);
        if (merged.lower < def.lower) lower_expansion_count_[key]++;
        if (merged.upper > def.upper) upper_expansion_count_[key]++;
        return merged != def;
    }

    Interval merged = it->second.convex_union(new_interval);
    if (merged == it->second) {
        return false;  // No change.
    }

    // Track which side(s) actually moved.
    if (merged.lower < it->second.lower) lower_expansion_count_[key]++;
    if (merged.upper > it->second.upper) upper_expansion_count_[key]++;

    it->second = merged;
    return true;
}

void NumericBoundsIndex::maybe_widen(int fluent_schema_id,
                                      const std::vector<int>& obj_indices,
                                      int threshold) {
    GroundKey key{fluent_schema_id, obj_indices};

    auto bounds_it = bounds_.find(key);
    if (bounds_it == bounds_.end()) return;

    Interval& iv = bounds_it->second;

    // Directional widening: only widen the side(s) that have been moving.
    // Frozen sides (from precompute_freezes) are never widened.
    if (!freeze_lower_.count(fluent_schema_id)) {
        auto lower_it = lower_expansion_count_.find(key);
        if (lower_it != lower_expansion_count_.end() &&
            lower_it->second >= threshold && !std::isinf(iv.lower)) {
            iv.lower = -std::numeric_limits<double>::infinity();
        }
    }

    if (!freeze_upper_.count(fluent_schema_id)) {
        auto upper_it = upper_expansion_count_.find(key);
        if (upper_it != upper_expansion_count_.end() &&
            upper_it->second >= threshold && !std::isinf(iv.upper)) {
            iv.upper = std::numeric_limits<double>::infinity();
        }
    }
}

bool NumericBoundsIndex::decompose_numeric_state_variable(
    ExprID eid, int& out_schema_id,
    std::vector<int>& out_obj_indices) const {

    const auto& pool = problem_.pool();

    // A STATE_VARIABLE's layout:
    //   children[0] = FLUENT_SYMBOL  (payload_string = fluent name)
    //   children[1..] = arguments    (CONSTANT, payload_string = object name)
    if (!pool.is_state_variable(eid)) return false;

    ExprID head_id = pool.head_symbol_id(eid);
    if (!pool.is_fluent_symbol(head_id)) return false;

    const std::string& fluent_name = pool.payload_string(head_id);
    const Fluent* fluent = problem_.find_fluent(fluent_name);
    if (!fluent) return false;

    // Only numeric fluents (functions, not predicates).
    if (fluent->is_predicate()) return false;

    out_schema_id = fluent->id();

    size_t num_args = pool.argument_count(eid);
    out_obj_indices.clear();
    out_obj_indices.reserve(num_args);

    for (size_t i = 0; i < num_args; ++i) {
        ExprID arg_id = pool.argument(eid, i);

        if (!pool.is_constant(arg_id) || !pool.payload_is_string(arg_id)) {
            return false;  // Not ground (has PARAMETER node).
        }

        const std::string& obj_name = pool.payload_string(arg_id);
        const Object* obj = problem_.find_object(obj_name);
        if (!obj) return false;

        size_t obj_index = static_cast<size_t>(obj - &problem_.objects()[0]);
        out_obj_indices.push_back(static_cast<int>(obj_index));
    }

    return true;
}

// ---------------------------------------------------------------------------
// precompute_freezes — lifted effect analysis for ceiling/floor freezing
// ---------------------------------------------------------------------------

void NumericBoundsIndex::precompute_freezes() {
    const auto& pool = problem_.pool();

    // Step 1: Identify which fluent schemas are modified by any effect.
    std::unordered_set<int> modified_schemas;

    struct EffectInfo {
        EffectExpression::Kind kind;
        ExprID value_id;
    };
    std::unordered_map<int, std::vector<EffectInfo>> effects_per_schema;

    for (size_t si = 0; si < problem_.action_count(); ++si) {
        const Action& schema = problem_.action(si);
        for (const auto& eff : schema.effects()) {
            const auto& ee = eff.effect_expression();
            ExprID fluent_eid = ee.fluent_id();

            int fid = extract_fluent_schema_id(fluent_eid, pool, problem_);
            if (fid < 0) continue;

            modified_schemas.insert(fid);
            effects_per_schema[fid].push_back({ee.kind(), ee.value_id()});
        }
    }

    // Step 2: Compute ranges for constant fluent schemas from initial state.
    // A fluent schema is "constant" if no effect modifies it.
    std::unordered_map<int, Interval> constant_ranges;

    for (const auto& fluent : problem_.fluents()) {
        if (fluent.is_function() && !modified_schemas.count(fluent.id())) {
            // Constant fluent — collect its range from initial state.
            constant_ranges.emplace(fluent.id(), Interval(0.0));  // Default [0,0].
        }
    }

    for (const auto& assignment : problem_.initial_state()) {
        int schema_id = -1;
        std::vector<int> obj_indices;
        if (decompose_numeric_state_variable(assignment.fluent_id(),
                                              schema_id, obj_indices)) {
            auto it = constant_ranges.find(schema_id);
            if (it != constant_ranges.end()) {
                double val = 0.0;
                ExprID vid = assignment.value_id();
                if (pool.payload_is_double(vid))
                    val = pool.payload_double(vid);
                else if (pool.payload_is_int(vid))
                    val = static_cast<double>(pool.payload_int(vid));
                it->second = it->second.convex_union(Interval(val));
            }
        }
    }

    // Step 3: For each non-constant fluent schema, classify effects.
    for (const auto& [fid, effects] : effects_per_schema) {
        bool can_freeze_upper = true;
        bool can_freeze_lower = true;

        for (const auto& [kind, value_id] : effects) {
            Interval val_range = evaluate_constant_expr_range(
                value_id, pool, problem_, constant_ranges);

            if (kind == EffectExpression::Kind::ASSIGN) {
                // Assign to a constant expression → introduces finite values.
                // If unbounded, the assign references non-constant fluents.
                if (val_range.is_unbounded()) {
                    can_freeze_upper = false;
                    can_freeze_lower = false;
                }
                // Bounded assign is fine — doesn't prevent freezing.
            } else if (kind == EffectExpression::Kind::INCREASE) {
                if (val_range.is_unbounded()) {
                    can_freeze_upper = false;
                    can_freeze_lower = false;
                } else {
                    // Positive delta can raise → can't freeze upper.
                    if (val_range.upper > 0) can_freeze_upper = false;
                    // Negative delta can lower → can't freeze lower.
                    if (val_range.lower < 0) can_freeze_lower = false;
                }
            } else if (kind == EffectExpression::Kind::DECREASE) {
                if (val_range.is_unbounded()) {
                    can_freeze_upper = false;
                    can_freeze_lower = false;
                } else {
                    // Positive decrease lowers → can't freeze lower.
                    if (val_range.upper > 0) can_freeze_lower = false;
                    // Negative decrease raises → can't freeze upper.
                    if (val_range.lower < 0) can_freeze_upper = false;
                }
            }

            if (!can_freeze_upper && !can_freeze_lower) break;
        }

        if (can_freeze_upper) freeze_upper_.insert(fid);
        if (can_freeze_lower) freeze_lower_.insert(fid);
    }

    // Log results.
    if (!freeze_upper_.empty() || !freeze_lower_.empty()) {
        size_t upper_count = freeze_upper_.size();
        size_t lower_count = freeze_lower_.size();
        Logger::instance().info("Grounding: freeze analysis — " +
            std::to_string(upper_count) + " upper frozen, " +
            std::to_string(lower_count) + " lower frozen");

        for (const auto& fluent : problem_.fluents()) {
            if (!fluent.is_function()) continue;
            bool fu = freeze_upper_.count(fluent.id());
            bool fl = freeze_lower_.count(fluent.id());
            if (fu || fl) {
                std::string sides;
                if (fu && fl) sides = "both";
                else if (fu) sides = "upper";
                else sides = "lower";
                Logger::instance().verbose("  " + fluent.name() + ": " + sides + " frozen");
            }
        }
    }
}

} // namespace rantanplan
