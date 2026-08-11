#include "protobuf_io.hpp"
#include "problem.hpp"
#include "plan.hpp"
#include "expr_enums.hpp"
#include <iostream>
#include <stdexcept>

namespace rantanplan {

// ============================================================================
// File-local helpers
// ============================================================================

static ExprID intern_expr(const pb::Expression& pb_expr,
                          const std::vector<Type>& types,
                          const std::unordered_map<std::string, const Type*>& type_map,
                          ExprPool& pool) {
    ExprNode node;

    // Convert kind
    int kind_value = static_cast<int>(pb_expr.kind());
    if (kind_value == 8) {
        throw std::runtime_error("Unsupported expression kind CONTAINER_ID (8) in protobuf input");
    }
    node.kind = kind_value;

    // Resolve type string to type_id
    const std::string& type_str = pb_expr.type();
    if (!type_str.empty()) {
        std::string resolved = type_str;
        if (type_str == "up:integer") resolved = "up:int";
        else if (type_str == "up:boolean") resolved = "up:bool";

        auto it = type_map.find(resolved);
        if (it == type_map.end() && resolved != type_str) {
            it = type_map.find(type_str);
        }
        if (it != type_map.end()) {
            const Type* found_type = it->second;
            for (size_t i = 0; i < types.size(); ++i) {
                if (&types[i] == found_type) {
                    node.type_id = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    if (pb_expr.has_atom()) {
        const auto& pb_atom = pb_expr.atom();
        ExprKind kind = static_cast<ExprKind>(pb_expr.kind());

        if (pb_atom.has_symbol()) {
            std::string symbol = pb_atom.symbol();
            if (kind == ExprKind::FUNCTION_SYMBOL || kind == ExprKind::FLUENT_SYMBOL) {
                symbol = map_up_operator(symbol);
            }
            node.payload = symbol;
            if (kind == ExprKind::FUNCTION_SYMBOL) {
                node.op = static_cast<int>(string_to_expr_operator(symbol));
            }
        } else if (pb_atom.has_int_()) {
            node.payload = pb_atom.int_();
        } else if (pb_atom.has_real()) {
            double val = static_cast<double>(pb_atom.real().numerator()) /
                         static_cast<double>(pb_atom.real().denominator());
            node.payload = val;
        } else if (pb_atom.has_boolean()) {
            node.payload = pb_atom.boolean();
        }
    }

    if (pb_expr.list_size() > 0) {
        ExprKind kind = static_cast<ExprKind>(pb_expr.kind());

        node.children.reserve(pb_expr.list_size());
        for (int i = 0; i < pb_expr.list_size(); ++i) {
            const auto& child_pb = pb_expr.list(i);

            if (i == 0 && kind == ExprKind::FUNCTION_APPLICATION &&
                child_pb.has_atom() && child_pb.atom().has_symbol()) {
                pb::Expression mapped_child = child_pb;
                std::string mapped = map_up_operator(child_pb.atom().symbol());
                mapped_child.mutable_atom()->set_symbol(mapped);
                ExprID child_id = intern_expr(mapped_child, types, type_map, pool);
                node.children.push_back(child_id);
                node.op = static_cast<int>(string_to_expr_operator(mapped));
            } else {
                node.children.push_back(intern_expr(child_pb, types, type_map, pool));
            }
        }
    }

    return pool.intern(std::move(node));
}

/// Context struct passed to helpers so they don't need Problem& access.
struct LoadContext {
    const std::vector<Type>& types;
    const std::unordered_map<std::string, const Type*>& type_map;
    ExprPool& pool;

    ExprID intern(const pb::Expression& expr) {
        return intern_expr(expr, types, type_map, pool);
    }

    const Type* find_type(const std::string& name) {
        auto it = type_map.find(name);
        return (it != type_map.end()) ? it->second : nullptr;
    }
};

static ExprID intern_precondition(LoadContext& ctx, const pb::Action& pb_action) {
    if (pb_action.conditions().empty()) {
        pb::Expression pb_true_expr;
        pb_true_expr.mutable_atom()->set_boolean(true);
        pb_true_expr.set_kind(pb::ExpressionKind::CONSTANT);
        pb_true_expr.set_type("up:bool");
        return ctx.intern(pb_true_expr);
    } else if (pb_action.conditions().size() == 1) {
        return ctx.intern(pb_action.conditions(0).cond());
    } else {
        pb::Expression pb_and_expr;
        pb_and_expr.set_kind(pb::ExpressionKind::FUNCTION_APPLICATION);
        pb_and_expr.set_type("up:bool");

        pb::Expression* and_symbol = pb_and_expr.add_list();
        and_symbol->mutable_atom()->set_symbol("and");
        and_symbol->set_kind(pb::ExpressionKind::FUNCTION_SYMBOL);
        and_symbol->set_type("up:bool");

        for (const auto& pb_condition : pb_action.conditions()) {
            pb::Expression* operand = pb_and_expr.add_list();
            *operand = pb_condition.cond();
        }

        return ctx.intern(pb_and_expr);
    }
}

static Action convert_action(LoadContext& ctx, const pb::Action& pb_action) {
    std::vector<Parameter> params;
    for (const auto& pb_param : pb_action.parameters()) {
        const Type* param_type = ctx.find_type(pb_param.type());
        params.emplace_back(pb_param.name(), param_type);
    }

    ExprID precondition_id = intern_precondition(ctx, pb_action);

    std::vector<Effect> effects;
    for (const auto& pb_effect : pb_action.effects()) {
        const auto& pb_ee = pb_effect.effect();

        auto kind = static_cast<EffectExpression::Kind>(pb_ee.kind());
        ExprID fluent_id = ctx.intern(pb_ee.fluent());
        ExprID value_id = ctx.intern(pb_ee.value());

        ExprID condition_id = EXPR_NULL;
        if (pb_ee.has_condition()) {
            condition_id = ctx.intern(pb_ee.condition());
        }

        std::vector<ExprID> forall_ids;
        for (const auto& var : pb_ee.forall()) {
            forall_ids.push_back(ctx.intern(var));
        }

        effects.emplace_back(EffectExpression(kind, fluent_id, value_id, condition_id,
                                               std::move(forall_ids), &ctx.pool));
    }

    return Action(pb_action.name(), std::move(params), precondition_id,
                  std::move(effects), &ctx.pool);
}

// ============================================================================
// Public API
// ============================================================================

Problem load_problem_from_protobuf(const ::Problem& pb_problem) {
    Problem problem;

    // --- Load types ---
    problem.types_->clear();
    problem.type_name_to_ptr_.clear();
    problem.types_->emplace_back("up:bool");
    problem.types_->emplace_back("up:int");
    problem.types_->emplace_back("up:integer");
    problem.types_->emplace_back("up:real");
    for (const auto& pb_type : pb_problem.types()) {
        problem.types_->emplace_back(pb_type.type_name());
        problem.types_->back().set_parent_name(pb_type.parent_type());
        // [XTS] Store element type and size so array_element_type_name() /
        // array_size() / set_element_type_name() work without string parsing.
        if (!pb_type.element_type().empty()) {
            problem.types_->back().set_element_info(pb_type.element_type(), static_cast<int64_t>(pb_type.size()));
        }
    }

    for (auto& type : *problem.types_) {
        problem.type_name_to_ptr_[type.name()] = &type;
    }
    problem.resolve_type_hierarchy();

    // Build context for helpers
    LoadContext ctx{*problem.types_, problem.type_name_to_ptr_, *problem.pool_};

    // --- Load objects ---
    for (const auto& pb_obj : pb_problem.objects()) {
        const Type* type = ctx.find_type(pb_obj.type());
        problem.objects_.emplace_back(pb_obj.name(), type);
    }
    problem.build_object_mappings();

    // --- Load fluents ---
    for (const auto& pb_fluent : pb_problem.fluents()) {
        std::vector<Parameter> params;
        for (const auto& pb_param : pb_fluent.parameters()) {
            const Type* param_type = ctx.find_type(pb_param.type());
            params.emplace_back(pb_param.name(), param_type);
        }
        const Type* value_type = ctx.find_type(pb_fluent.value_type());
        if (!value_type) {
            std::cerr << "ERROR: Could not find type '" << pb_fluent.value_type()
                << "' for fluent '" << pb_fluent.name() << "'" << std::endl;
        }
        problem.fluents_.emplace_back(pb_fluent.name(), value_type, params);
    }
    problem.build_fluent_mappings();

    // --- Load actions ---
    for (const auto& pb_action : pb_problem.actions()) {
        problem.actions_.push_back(convert_action(ctx, pb_action));
        problem.actions_.back().set_id(problem.actions_.size() - 1);
    }
    problem.build_action_mappings();

    // --- Load initial state ---
    for (const auto& pb_assignment : pb_problem.initial_state()) {
        ExprID fluent_id = ctx.intern(pb_assignment.fluent());
        ExprID value_id = ctx.intern(pb_assignment.value());
        problem.initial_state_.emplace_back(fluent_id, value_id, &problem.pool());
    }

    // --- Load goals ---
    for (const auto& pb_goal : pb_problem.goals()) {
        problem.goals_.emplace_back(ctx.intern(pb_goal.goal()), &problem.pool());
    }

    // --- Load metrics (action costs) ---
    for (const auto& pb_metric : pb_problem.metrics()) {
        if (pb_metric.kind() == ::Metric::MINIMIZE_ACTION_COSTS) {
            problem.set_metric_kind(MetricKind::MINIMIZE_ACTION_COSTS);

            // Apply per-action costs
            for (auto& action : problem.actions_) {
                auto it = pb_metric.action_costs().find(action.name());
                if (it != pb_metric.action_costs().end()) {
                    action.set_cost_id(ctx.intern(it->second));
                } else if (pb_metric.has_default_action_cost()) {
                    action.set_cost_id(ctx.intern(pb_metric.default_action_cost()));
                }
            }
            break;  // only process the first MINIMIZE_ACTION_COSTS metric
        } else if (pb_metric.kind() == ::Metric::MINIMIZE_SEQUENTIAL_PLAN_LENGTH) {
            problem.set_metric_kind(MetricKind::MINIMIZE_PLAN_LENGTH);
            break;
        }
    }

    // Ensure every action has a cost_id. Actions without explicit costs
    // (e.g., MINIMIZE_PLAN_LENGTH or no metric) default to unit cost = 1.
    {
        ExprID unit_cost = problem.pool_->intern_int_constant(1);
        for (auto& action : problem.actions_) {
            if (!action.has_explicit_cost()) {
                action.set_cost_id(unit_cost);
            }
        }
    }

    problem.collect_grounded_fluents();
    return problem;
}

pb::Plan plan_to_protobuf(const Plan& plan) {
    pb::Plan pb_plan_msg;

    for (const Action* action : plan.actions()) {
        pb::ActionInstance* pb_action_instance = pb_plan_msg.add_actions();
        pb_action_instance->set_action_name(action->name());

        for (const Parameter& param : action->parameters()) {
            pb::Atom* pb_param = pb_action_instance->add_parameters();
            const Type* ptype = param.type();
            if (ptype && ptype->bounded_int_ancestor()) {
                pb_param->set_int_(std::stoll(param.name()));
            } else {
                pb_param->set_symbol(param.name());
            }
        }
    }

    return pb_plan_msg;
}

} // namespace rantanplan
