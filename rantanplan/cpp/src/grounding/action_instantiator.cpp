#include "action_instantiator.hpp"
#include "../problem/substitution.hpp"

namespace rantanplan {

// ---------------------------------------------------------------------------
// Helper: build an object-constant ExprID from an object index.
// ---------------------------------------------------------------------------

static ExprID make_object_constant(ExprPool& pool,
                                    const Problem& problem,
                                    int obj_idx) {
    const Object& obj = problem.object(obj_idx);

    ExprNode node;
    node.kind = static_cast<int>(ExprKind::CONSTANT);
    // NOTE: Leave node.op as default (-1) to match intern_expr() in protobuf_io.cpp,
    // which does NOT set op for CONSTANT nodes.

    // Derive type_id via pointer arithmetic (Type* points into types_ vector).
    const Type* obj_type = obj.type();
    if (obj_type) {
        node.type_id = static_cast<int>(obj_type - &problem.types()[0]);
    }

    node.payload = obj.name();
    return pool.intern(node);
}

// Helper: build an integer-constant ExprID from a value.
static ExprID make_int_constant(ExprPool& pool, int64_t value) {
    return pool.intern_int_constant(value);
}

// ---------------------------------------------------------------------------
// Helper: build a Substitution (param_name → constant ExprID).
// ---------------------------------------------------------------------------

static Substitution build_substitution(ExprPool& pool,
                                        const Problem& problem,
                                        const Action& lifted_action,
                                        const PartialBinding& binding) {
    Substitution subst;
    for (const auto& [param_idx, obj_or_int_val] : binding) {
        const std::string& param_name = lifted_action.parameter(param_idx).name();
        const Type* ptype = lifted_action.parameter(param_idx).type();

        ExprID constant_id;
        if (ptype && ptype->bounded_int_ancestor()) {
            constant_id = make_int_constant(pool, static_cast<int64_t>(obj_or_int_val));
        } else {
            constant_id = make_object_constant(pool, problem, obj_or_int_val);
        }
        subst[param_name] = constant_id;
    }
    return subst;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

Action instantiate_action(ExprPool& pool,
                          const Problem& problem,
                          const Action& lifted_action,
                          const PartialBinding& binding) {
    Substitution subst = build_substitution(pool, problem, lifted_action, binding);

    // Create ground action with base name (e.g., "board") and parameters
    // holding the bound object names (e.g., "person2", "plane1", "city0").
    // This ensures Plan::to_protobuf() writes a protobuf that the Python
    // reader can resolve against the original lifted problem.
    Action ground(lifted_action.name());
    ground.set_pool(&pool);

    for (size_t i = 0; i < lifted_action.parameter_count(); ++i) {
        auto it = binding.find(static_cast<int>(i));
        if (it != binding.end()) {
            const Parameter& lparam = lifted_action.parameter(i);
            const Type* ptype = lparam.type();
            if (ptype && ptype->bounded_int_ancestor()) {
                // Bounded-int parameter: name is the integer value as a string.
                ground.add_parameter(Parameter(std::to_string(it->second), ptype));
            } else {
                const Object& obj = problem.object(it->second);
                ground.add_parameter(Parameter(obj.name(), obj.type()));
            }
        }
    }

    // Substitute precondition.
    if (lifted_action.has_precondition()) {
        ExprID ground_pre = substitute(pool, lifted_action.precondition_id(), subst);
        ground.set_precondition_id(ground_pre);
    }

    // Substitute cost expression.
    if (lifted_action.has_explicit_cost()) {
        ExprID ground_cost = substitute(pool, lifted_action.cost_id(), subst);
        ground.set_cost_id(ground_cost);
    }

    // Substitute each effect.
    for (const auto& eff : lifted_action.effects()) {
        const auto& ee = eff.effect_expression();

        ExprID ground_fluent = substitute(pool, ee.fluent_id(), subst);
        ExprID ground_value  = substitute(pool, ee.value_id(), subst);
        ExprID ground_cond   = ee.is_conditional()
            ? substitute(pool, ee.condition_id(), subst)
            : EXPR_NULL;

        EffectExpression ground_ee(ee.kind(), ground_fluent, ground_value,
                                    ground_cond, &pool);
        ground.add_effect(Effect(ground_ee));
    }

    return ground;
}

} // namespace rantanplan

