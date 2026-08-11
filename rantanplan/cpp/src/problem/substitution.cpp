#include "substitution.hpp"

namespace rantanplan {

ExprID substitute_vars(ExprPool& pool, ExprID expr, const Substitution& var_subst) {
    // Base case: invalid expression passes through unchanged.
    if (!expr.valid()) return expr;

    const ExprNode& node_ref = pool.get(expr);

    // Leaf: VARIABLE node: look up in substitution map.
    if (pool.is_variable(expr)) {
        if (std::holds_alternative<std::string>(node_ref.payload)) {
            const auto& var_name = std::get<std::string>(node_ref.payload);
            auto it = var_subst.find(var_name);
            if (it != var_subst.end()) return it->second;
        }
        return expr;  // variable not in substitution: return unchanged
    }

    // Leaf: any non-VARIABLE leaf (CONSTANT, FLUENT_SYMBOL, etc.)
    if (node_ref.children.empty()) return expr;

    // Internal node: copy children list and node data before recursing
    std::vector<ExprID> orig_children = node_ref.children;
    int orig_kind   = node_ref.kind;
    int orig_op     = node_ref.op;
    int orig_type   = node_ref.type_id;
    ExprPayload orig_payload = node_ref.payload;

    bool any_changed = false;
    std::vector<ExprID> new_children;
    new_children.reserve(orig_children.size());
    for (ExprID child_id : orig_children) {
        ExprID new_child = substitute_vars(pool, child_id, var_subst);
        new_children.push_back(new_child);
        if (new_child != child_id) any_changed = true;
    }

    // No child changed: reuse the existing node.
    if (!any_changed) return expr;

    // Rebuild
    ExprNode new_node;
    new_node.kind     = orig_kind;
    new_node.op       = orig_op;
    new_node.type_id  = orig_type;
    new_node.children = std::move(new_children);
    new_node.payload  = std::move(orig_payload);
    return pool.intern(std::move(new_node));
}

ExprID substitute(ExprPool& pool, ExprID expr, const Substitution& subst) {
    // Base case: invalid expression passes through unchanged.
    if (!expr.valid()) return expr;

    // IMPORTANT: We must not hold references into pool.nodes_ across calls that
    // may add new nodes (recursive substitute → intern can reallocate the vector).
    // So we copy the node data we need up-front.

    const ExprNode& node_ref = pool.get(expr);

    // ---------------------------------------------------------------------------
    // Leaf: PARAMETER node — look up in substitution map.
    // ---------------------------------------------------------------------------
    if (pool.is_parameter(expr)) {
        if (std::holds_alternative<std::string>(node_ref.payload)) {
            const auto& param_name = std::get<std::string>(node_ref.payload);
            auto it = subst.find(param_name);
            if (it != subst.end()) {
                return it->second;
            }
        }
        return expr;  // parameter not in substitution — return unchanged
    }

    // ---------------------------------------------------------------------------
    // Leaf: any non-PARAMETER leaf (CONSTANT, FLUENT_SYMBOL, etc.)
    // ---------------------------------------------------------------------------
    if (node_ref.children.empty()) {
        return expr;
    }

    // ---------------------------------------------------------------------------
    // Internal node: copy what we need BEFORE recursing, since recursive calls
    // may invalidate references into the pool's node vector.
    // ---------------------------------------------------------------------------
    // Copy children list and node metadata before recursing.
    std::vector<ExprID> orig_children = node_ref.children;
    int orig_kind = node_ref.kind;
    int orig_op = node_ref.op;
    int orig_type_id = node_ref.type_id;
    ExprPayload orig_payload = node_ref.payload;
    // After this point, node_ref may be dangling — do not use it.

    bool any_changed = false;
    std::vector<ExprID> new_children;
    new_children.reserve(orig_children.size());

    for (ExprID child_id : orig_children) {
        ExprID new_child = substitute(pool, child_id, subst);
        new_children.push_back(new_child);
        if (new_child != child_id) {
            any_changed = true;
        }
    }

    if (!any_changed) {
        return expr;
    }

    ExprNode new_node;
    new_node.kind = orig_kind;
    new_node.op = orig_op;
    new_node.type_id = orig_type_id;
    new_node.children = std::move(new_children);
    new_node.payload = std::move(orig_payload);

    return pool.intern(std::move(new_node));
}

} // namespace rantanplan
