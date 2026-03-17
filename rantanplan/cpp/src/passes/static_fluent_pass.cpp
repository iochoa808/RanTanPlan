#include "static_fluent_pass.hpp"
#include "../util/logger.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/stats.hpp"
#include <cmath>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace rantanplan {

// ============================================================================
// Helpers: extract numeric value from a CONSTANT ExprID
// ============================================================================

static std::optional<double> try_get_numeric(const ExprPool& pool, ExprID id) {
    if (!id.valid() || !pool.is_constant(id)) return std::nullopt;
    if (pool.payload_is_double(id)) return pool.payload_double(id);
    if (pool.payload_is_int(id))    return static_cast<double>(pool.payload_int(id));
    return std::nullopt;
}

// ============================================================================
// Constant folding for arithmetic operations
// ============================================================================

// Try to fold a FUNCTION_APPLICATION whose arguments (children[1..]) are all
// numeric constants. Returns a folded CONSTANT ExprID or EXPR_NULL.
static ExprID try_fold_arithmetic(ExprPool& pool, ExprOperator op,
                                   const std::vector<ExprID>& children, int type_id) {
    // children[0] = head symbol; children[1..] = arguments
    if (children.size() < 2) return EXPR_NULL;

    // Collect numeric values from arguments
    std::vector<double> vals;
    for (size_t i = 1; i < children.size(); ++i) {
        auto v = try_get_numeric(pool, children[i]);
        if (!v) return EXPR_NULL;
        vals.push_back(*v);
    }

    double result;
    switch (op) {
        case ExprOperator::PLUS:
            result = 0.0;
            for (double v : vals) result += v;
            break;
        case ExprOperator::MINUS:
            if (vals.size() == 1) { result = -vals[0]; break; }
            result = vals[0] - vals[1];
            break;
        case ExprOperator::MULTIPLY:
            result = 1.0;
            for (double v : vals) result *= v;
            break;
        case ExprOperator::DIVIDE:
            if (vals.size() < 2 || vals[1] == 0.0) return EXPR_NULL;
            result = vals[0] / vals[1];
            break;
        case ExprOperator::MODULO:
            if (vals.size() < 2 || vals[1] == 0.0) return EXPR_NULL;
            result = std::fmod(vals[0], vals[1]);
            break;
        case ExprOperator::ABSOLUTE:
            result = std::abs(vals[0]);
            break;
        case ExprOperator::MAXIMUM:
            result = vals[0];
            for (size_t i = 1; i < vals.size(); ++i) result = std::max(result, vals[i]);
            break;
        case ExprOperator::MINIMUM:
            result = vals[0];
            for (size_t i = 1; i < vals.size(); ++i) result = std::min(result, vals[i]);
            break;
        default:
            return EXPR_NULL;
    }

    return pool.intern_double_constant(result, type_id);
}

// ============================================================================
// Constant folding for comparison operations
// ============================================================================

static ExprID try_fold_comparison(ExprPool& pool, ExprOperator op,
                                   const std::vector<ExprID>& children) {
    if (children.size() != 3) return EXPR_NULL;  // head + 2 args

    auto lhs = try_get_numeric(pool, children[1]);
    auto rhs = try_get_numeric(pool, children[2]);
    if (!lhs || !rhs) return EXPR_NULL;

    bool result;
    switch (op) {
        case ExprOperator::EQUALS:        result = (*lhs == *rhs); break;
        case ExprOperator::LESS_EQUAL:    result = (*lhs <= *rhs); break;
        case ExprOperator::LESS_THAN:     result = (*lhs <  *rhs); break;
        case ExprOperator::GREATER_EQUAL: result = (*lhs >= *rhs); break;
        case ExprOperator::GREATER_THAN:  result = (*lhs >  *rhs); break;
        default: return EXPR_NULL;
    }

    return pool.intern_bool_constant(result);
}

// ============================================================================
// Constant folding for boolean operations (full and partial)
// ============================================================================

// Returns a simplified ExprID, or EXPR_NULL if no simplification possible.
// Handles both full folding (all constant args) and partial folding
// (e.g., AND(true, X) → X).
static ExprID try_fold_boolean(ExprPool& pool, ExprOperator op,
                                const std::vector<ExprID>& children, int type_id) {
    if (children.size() < 2) return EXPR_NULL;
    ExprID head = children[0];

    if (op == ExprOperator::NOT && children.size() == 2) {
        if (pool.is_true_constant(children[1]))  return pool.intern_bool_constant(false);
        if (pool.is_false_constant(children[1])) return pool.intern_bool_constant(true);
        return EXPR_NULL;
    }

    if (op == ExprOperator::AND) {
        // Filter out true args, short-circuit on false
        std::vector<ExprID> kept;
        for (size_t i = 1; i < children.size(); ++i) {
            if (pool.is_true_constant(children[i])) continue;  // skip true
            if (pool.is_false_constant(children[i])) return pool.intern_bool_constant(false);
            kept.push_back(children[i]);
        }
        if (kept.empty()) return pool.intern_bool_constant(true);
        if (kept.size() == 1) return kept[0];
        if (kept.size() < children.size() - 1) {
            // Build simplified AND with fewer arguments
            ExprNode node;
            node.kind = static_cast<int>(ExprKind::FUNCTION_APPLICATION);
            node.op = static_cast<int>(ExprOperator::AND);
            node.type_id = type_id;
            node.children.push_back(head);
            for (ExprID k : kept) node.children.push_back(k);
            return pool.intern(std::move(node));
        }
        return EXPR_NULL;
    }

    if (op == ExprOperator::OR) {
        // Filter out false args, short-circuit on true
        std::vector<ExprID> kept;
        for (size_t i = 1; i < children.size(); ++i) {
            if (pool.is_false_constant(children[i])) continue;  // skip false
            if (pool.is_true_constant(children[i])) return pool.intern_bool_constant(true);
            kept.push_back(children[i]);
        }
        if (kept.empty()) return pool.intern_bool_constant(false);
        if (kept.size() == 1) return kept[0];
        if (kept.size() < children.size() - 1) {
            ExprNode node;
            node.kind = static_cast<int>(ExprKind::FUNCTION_APPLICATION);
            node.op = static_cast<int>(ExprOperator::OR);
            node.type_id = type_id;
            node.children.push_back(head);
            for (ExprID k : kept) node.children.push_back(k);
            return pool.intern(std::move(node));
        }
        return EXPR_NULL;
    }

    if (op == ExprOperator::IMPLIES && children.size() == 3) {
        if (pool.is_false_constant(children[1])) return pool.intern_bool_constant(true);
        if (pool.is_true_constant(children[1]))  return children[2];
        if (pool.is_true_constant(children[2]))  return pool.intern_bool_constant(true);
    }

    return EXPR_NULL;
}

// ============================================================================
// Combined substitute + constant fold walker
// ============================================================================

static ExprID simplify_expr(ExprPool& pool, ExprID expr,
                             const std::unordered_map<ExprID, ExprID>& static_values) {
    if (!expr.valid()) return expr;

    // Direct substitution: if this is a static fluent → return its constant value
    if (auto it = static_values.find(expr); it != static_values.end()) {
        return it->second;
    }

    const ExprNode& node_ref = pool.get(expr);

    // Leaf → unchanged
    if (node_ref.children.empty()) return expr;

    // Copy needed fields before recursing (pool.intern may reallocate nodes_)
    auto orig_children = node_ref.children;
    int orig_kind = node_ref.kind;
    int orig_op = node_ref.op;
    int orig_type_id = node_ref.type_id;
    auto orig_payload = node_ref.payload;
    // node_ref may be dangling after this point

    // Recurse on all children
    bool any_changed = false;
    std::vector<ExprID> new_children;
    new_children.reserve(orig_children.size());
    for (ExprID child : orig_children) {
        ExprID nc = simplify_expr(pool, child, static_values);
        new_children.push_back(nc);
        if (nc != child) any_changed = true;
    }

    if (!any_changed) return expr;

    // Try constant folding on FUNCTION_APPLICATION nodes
    if (static_cast<ExprKind>(orig_kind) == ExprKind::FUNCTION_APPLICATION) {
        auto op = static_cast<ExprOperator>(orig_op);

        if (is_arithmetic_operator(op)) {
            ExprID folded = try_fold_arithmetic(pool, op, new_children, orig_type_id);
            if (folded.valid()) return folded;
        } else if (is_comparison_operator(op)) {
            ExprID folded = try_fold_comparison(pool, op, new_children);
            if (folded.valid()) return folded;
        } else if (is_logical_operator(op)) {
            ExprID folded = try_fold_boolean(pool, op, new_children, orig_type_id);
            if (folded.valid()) return folded;
        }
    }

    // Intern the new node with substituted children
    ExprNode new_node;
    new_node.kind = orig_kind;
    new_node.op = orig_op;
    new_node.type_id = orig_type_id;
    new_node.children = std::move(new_children);
    new_node.payload = std::move(orig_payload);
    return pool.intern(std::move(new_node));
}

// ============================================================================
// Pass implementation
// ============================================================================

void StaticFluentPass::apply(PipelineResult& result) const {
    ScopedTimer timer("static_fluent.time_ms");
    const auto& problem = result.problem;
    ExprPool& pool = *problem.pool_ptr();

    // Step 1: Identify fluents modified by any effect
    std::unordered_set<ExprID> modified_fluents;
    for (const auto& action : problem.actions()) {
        for (const auto& effect : action.effects()) {
            modified_fluents.insert(effect.effect_expression().fluent_id());
        }
    }

    // Step 2: Static fluents = grounded fluents NOT in modified set
    std::unordered_set<ExprID> static_fluent_set;
    for (ExprID gf : problem.grounded_fluents()) {
        if (!modified_fluents.count(gf)) {
            static_fluent_set.insert(gf);
        }
    }

    if (static_fluent_set.empty()) {
        Logger::instance().component(VerbosityLevel::INFO, name(), {
            {"status", "no static fluents found"}
        });
        return;
    }

    // Step 3: Build value map from initial state
    std::unordered_map<ExprID, ExprID> static_values;
    for (const auto& assignment : problem.initial_state()) {
        if (static_fluent_set.count(assignment.fluent_id())) {
            static_values[assignment.fluent_id()] = assignment.value_id();
        }
    }

    // Step 4: Simplify all action expressions
    std::vector<Action> new_actions;
    new_actions.reserve(problem.actions().size());

    for (const auto& action : problem.actions()) {
        Action new_action = action;

        // Simplify precondition
        if (new_action.has_precondition()) {
            ExprID new_pre = simplify_expr(pool, action.precondition_id(), static_values);
            if (new_pre != action.precondition_id()) {
                new_action.set_precondition_id(new_pre);
            }
        }

        // Simplify effects
        for (auto& effect : new_action.mutable_effects()) {
            const auto& ee = effect.effect_expression();
            ExprID new_value = simplify_expr(pool, ee.value_id(), static_values);
            ExprID new_cond = ee.is_conditional()
                ? simplify_expr(pool, ee.condition_id(), static_values)
                : ee.condition_id();

            if (new_value != ee.value_id() || new_cond != ee.condition_id()) {
                EffectExpression new_ee(ee.kind(), ee.fluent_id(), new_value, new_cond, &pool);
                effect.set_effect_expression(new_ee);
            }
        }

        // Simplify cost expression
        if (new_action.has_explicit_cost()) {
            ExprID new_cost = simplify_expr(pool, action.cost_id(), static_values);
            if (new_cost != action.cost_id()) {
                new_action.set_cost_id(new_cost);
            }
        }

        new_actions.push_back(std::move(new_action));
    }

    // Step 5: Simplify goals
    std::vector<Goal> new_goals;
    new_goals.reserve(problem.goals().size());
    for (const auto& goal : problem.goals()) {
        Goal new_goal = goal;
        ExprID new_gid = simplify_expr(pool, goal.goal_id(), static_values);
        if (new_gid != goal.goal_id()) {
            new_goal.set_goal_id(new_gid);
        }
        new_goals.push_back(std::move(new_goal));
    }

    // Step 6: Filter initial state (remove static fluent assignments)
    std::vector<Assignment> new_initial_state;
    new_initial_state.reserve(problem.initial_state().size());
    for (const auto& assignment : problem.initial_state()) {
        if (!static_fluent_set.count(assignment.fluent_id())) {
            new_initial_state.push_back(assignment);
        }
    }

    // Step 7: Build new problem (re-collects grounded fluents automatically)
    size_t old_fluent_count = problem.grounded_fluent_count();
    result.problem = problem.with_simplified(
        std::move(new_actions), std::move(new_goals), std::move(new_initial_state));
    size_t new_fluent_count = result.problem.grounded_fluent_count();

    Stats::instance().set("static_fluent.removed",
                           static_cast<double>(static_values.size()));

    Logger::instance().component(VerbosityLevel::INFO, name(), {
        {"time", std::to_string(static_cast<int>(timer.elapsed_ms())) + "ms"},
        {"static fluents", std::to_string(static_values.size())},
        {"grounded fluents", std::to_string(old_fluent_count) + " -> " +
                             std::to_string(new_fluent_count)}
    });
}

} // namespace rantanplan
