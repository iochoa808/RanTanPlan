#include "grounded_encoding_visitor.hpp"
#include "z3_variable_factory.hpp"
#include "array_domain_utils.hpp" // [XTS-UnFun]
#include "../problem/fluent.hpp"
#include "../problem/effect_expression.hpp"
#include "../problem/substitution.hpp"
#include <stdexcept>
#include <functional>
#include <iostream>

namespace rantanplan {

// [XTS-UnFun] Forward declaration: value_type_of_expr is defined further down in this
// file (near elem_type_of_set_expr) but is also needed by convert_node's whole-array/set
// EQUALS interception, which appears earlier in the file.
static const Type* value_type_of_expr(ExprID eid, const ExprPool& pool, const Problem* problem);

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

// [XTS] COUNT: sums booleans as 0/1 integers — encodes "how many of these conditions hold".
z3::expr GroundedEncodingVisitor::handle_count(const std::vector<z3::expr>& args) {
    if (args.empty()) {
        return ctx_.int_val(0);
    }
    z3::expr result = ctx_.int_val(0);
    for (const auto& arg : args) {
        result = result + z3::ite(arg, ctx_.int_val(1), ctx_.int_val(0));
    }
    return result;
}

// ============================================================================
// ExprID-based conversion: walks ExprNode directly via ExprPool
// ============================================================================

// Public entry point. Sets current_timestep_ so all STATE_VARIABLE lookups inside
// convert_node() use the right time index, then restores the saved value so a
// re-entrant call cannot clobber an outer visit's timestep.
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

// [XTS] See header for the contract.
//
// Forall/exists precondition: stored as FUNCTION_APPLICATION with op=UNKNOWN and head
// symbol "up:forall" or "up:exists". Forall expands to AND of body instances; exists
// expands to OR.
// Pool structure: arguments = [range_var(var, lo, hi), ..., body]
// Example: (forall (?i - [0,3]) (= (fuel ?i) 0))
//   → body[?i=0] ∧ body[?i=1] ∧ body[?i=2] ∧ body[?i=3]
z3::expr GroundedEncodingVisitor::convert_quantifier(ExprID id) {
    const ExprPool& pool = problem_->pool();

    ExprID head = pool.head_symbol_id(id);
    if (!pool.payload_is_string(head)) {
        throw std::runtime_error(
            "Unsupported UNKNOWN operator in ExprID conversion (no string head)");
    }
    const std::string head_sym = pool.payload_string(head);
    const bool is_forall = (head_sym == "up:forall");
    const bool is_exists = (head_sym == "up:exists");
    if (!is_forall && !is_exists) {
        throw std::runtime_error(
            "Unsupported UNKNOWN operator in ExprID conversion "
            "(head symbol: " + head_sym + ")");
    }

    std::vector<ExprID> args;
    for (ExprID a : pool.arguments(id)) args.push_back(a);
    if (args.size() < 2) {
        throw std::runtime_error(
            std::string(is_forall ? "Malformed forall" : "Malformed exists") +
            ": expected at least [range_var, body]");
    }
    ExprID body_id = args.back();
    std::vector<ExprID> range_vars(args.begin(), args.end() - 1);

    // Resolve int type_id for synthesised CONSTANT nodes.
    int int_type_id = -1;
    const Type* int_type = problem_->find_type("up:int");
    if (int_type) {
        const auto& types = problem_->types();
        for (size_t i = 0; i < types.size(); ++i)
            if (&types[i] == int_type) { int_type_id = static_cast<int>(i); break; }
    }

    ExprPool& mpool = *problem_->pool_ptr();
    std::vector<z3::expr> instances;
    Substitution var_subst;

    std::function<void(size_t)> expand = [&](size_t vi) {
        if (vi == range_vars.size()) {
            ExprID subst_body = substitute_vars(mpool, body_id, var_subst);
            instances.push_back(convert_node(subst_body));
            return;
        }
        ExprID rv = range_vars[vi];

        if (pool.is_variable(rv)) {
            // UP object-typed quantifier variable (e.g. from INTEGERS_REMOVING /
            // USERTYPE_FLUENTS_REMOVING).  Enumerate all objects of the declared type.
            // Copy name now: mpool.intern() inside the loop can reallocate nodes_.
            const std::string var_name = pool.payload_string(rv);
            int rv_type_id = pool.type_id(rv);
            const Type* obj_type =
                (rv_type_id >= 0 &&
                 rv_type_id < static_cast<int>(problem_->types().size()))
                ? &problem_->types()[rv_type_id] : nullptr;
            if (!obj_type) {
                throw std::runtime_error(
                    "Object-typed quantifier variable '" + var_name +
                    "' has unknown type (type_id=" + std::to_string(rv_type_id) + ")");
            }
            for (int obj_idx : problem_->objects_of_type(obj_type)) {
                ExprNode cn;
                cn.kind    = static_cast<int>(ExprKind::CONSTANT);
                cn.payload = problem_->objects()[obj_idx].name();
                cn.type_id = rv_type_id;
                ExprID new_const = mpool.intern(std::move(cn));
                var_subst[var_name] = new_const;
                expand(vi + 1);
            }
            var_subst.erase(var_name);
        } else {
            // XTS bounded-integer range variable triple (var, lo, hi).
            ExprID var_expr = pool.argument(rv, 0);
            ExprID lo_id   = pool.argument(rv, 1);
            ExprID hi_id   = pool.argument(rv, 2);
            // Copy: pool.intern() inside the loop can reallocate nodes_,
            // invalidating any reference into pool.payload_string().
            const std::string var_name = pool.payload_string(var_expr);

            auto read_bound = [&](ExprID bid) -> int64_t {
                if (pool.payload_is_int(bid))    return pool.payload_int(bid);
                if (pool.payload_is_double(bid)) return static_cast<int64_t>(pool.payload_double(bid));
                throw std::runtime_error(
                    "Quantifier range_var bound for '" + var_name + "' is not a ground integer");
            };
            int64_t lo = read_bound(lo_id);
            int64_t hi = read_bound(hi_id);

            for (int64_t v = lo; v <= hi; ++v) {
                ExprNode cn;
                cn.kind    = static_cast<int>(ExprKind::CONSTANT);
                cn.payload = v;
                cn.type_id = int_type_id;
                ExprID new_const = mpool.intern(std::move(cn));
                var_subst[var_name] = new_const;
                expand(vi + 1);
            }
            var_subst.erase(var_name);
        }
    };

    expand(0);
    // forall = AND over instances (empty → true); exists = OR (empty → false)
    return is_forall ? handle_and(instances) : handle_or(instances);
}

// [XTS] See header for the contract.
//
// SET_MEMBER: XTS sets are int→bool Z3 arrays; membership is select(set, elem) == true.
// SET_SUBSETEQ / SET_DISJOINT / SET_CARDINALITY: no ∀ quantifiers — enumerate the
// finite element domain and emit a conjunction or integer sum, keeping the theory QF_AX.
//   subseteq(A,B): AND_{e in dom} (select(A,e) → select(B,e))
//   disjoint(A,B): AND_{e in dom} ¬(select(A,e) ∧ select(B,e))
//   |S|:           Σ_{e in dom} ite(select(S,e), 1, 0)
std::optional<z3::expr> GroundedEncodingVisitor::try_convert_set_op(ExprID id) {
    const ExprPool& pool = problem_->pool();
    const ExprOperator op = pool.op(id);

    if (op == ExprOperator::SET_MEMBER) {
        if (pool.argument_count(id) != 2)
            throw std::runtime_error("SET_MEMBER expects 2 arguments");
        z3::expr elem_z3 = convert_node(pool.argument(id, 0));
        return convert_membership(elem_z3, pool.argument(id, 1));
    }

    if (op == ExprOperator::SET_SUBSETEQ) {
        if (pool.argument_count(id) != 2)
            throw std::runtime_error("SET_SUBSETEQ expects 2 arguments");
        ExprID A = pool.argument(id, 0);
        ExprID B = pool.argument(id, 1);
        const Type* elem_type = elem_type_of_set_expr(A);
        if (!elem_type)
            throw std::runtime_error("Cannot determine element type for SET_SUBSETEQ");
        // A ⊆ B  ≡  ∀e. e∈A → e∈B, expanded over the finite element domain.
        z3::expr_vector conjuncts(ctx_);
        for_each_set_element(elem_type, [&](const z3::expr& e) {
            conjuncts.push_back(
                z3::implies(convert_membership(e, A), convert_membership(e, B)));
        });
        return conjuncts.empty() ? ctx_.bool_val(true) : z3::mk_and(conjuncts);
    }

    if (op == ExprOperator::SET_DISJOINT) {
        if (pool.argument_count(id) != 2)
            throw std::runtime_error("SET_DISJOINT expects 2 arguments");
        ExprID A = pool.argument(id, 0);
        ExprID B = pool.argument(id, 1);
        const Type* elem_type = elem_type_of_set_expr(A);
        if (!elem_type)
            throw std::runtime_error("Cannot determine element type for SET_DISJOINT");
        // disjoint(A,B) ≡ ∀e. ¬(e∈A ∧ e∈B), expanded over the finite element domain.
        z3::expr_vector conjuncts(ctx_);
        for_each_set_element(elem_type, [&](const z3::expr& e) {
            conjuncts.push_back(!(convert_membership(e, A) && convert_membership(e, B)));
        });
        return conjuncts.empty() ? ctx_.bool_val(true) : z3::mk_and(conjuncts);
    }

    if (op == ExprOperator::SET_CARDINALITY) {
        if (pool.argument_count(id) != 1)
            throw std::runtime_error("SET_CARDINALITY expects 1 argument");
        ExprID set_id = pool.argument(id, 0);
        const Type* elem_type = elem_type_of_set_expr(set_id);
        if (!elem_type)
            throw std::runtime_error("Cannot determine element type for SET_CARDINALITY");
        // |S| ≡ Σ over the finite element domain of ite(e∈S, 1, 0).
        z3::expr total = variable_factory_->make_numeric_val(static_cast<int64_t>(0));
        for_each_set_element(elem_type, [&](const z3::expr& e) {
            z3::expr mem = convert_membership(e, set_id);
            total = total + z3::ite(mem,
                variable_factory_->make_numeric_val(static_cast<int64_t>(1)),
                variable_factory_->make_numeric_val(static_cast<int64_t>(0)));
        });
        return total;
    }

    return std::nullopt;
}

// [XTS-UnFun] See header for the contract.
std::optional<z3::expr> GroundedEncodingVisitor::try_convert_uf_array_op(ExprID id) {
    const ExprPool& pool = problem_->pool();
    const ExprOperator op = pool.op(id);

    // ARRAY_READ under UF mode: if the (possibly nested) read chain's root is a plain
    // array-typed STATE_VARIABLE, apply that fluent's UF function directly to the full
    // peeled index tuple — one function application instead of materializing the base
    // array as a Theory value and nested-selecting into it. Returns nullopt (so the
    // caller falls through to the generic nested-select ARRAY_READ case) for Theory
    // mode, or when the root isn't a plain SV (e.g. a literal from static-fluent
    // substitution — no UF function exists for it, so Theory-style select on the
    // materialized literal is correct there).
    if (op == ExprOperator::ARRAY_READ) {
        std::vector<ExprID> idx_ids;
        ExprID root = peel_array_read_chain(id, pool, idx_ids);
        if (variable_factory_->uf_mode() && pool.is_state_variable(root)) {
            Fluent fl = build_grounded_fluent(root);
            const Type* vt = fl.value_type();
            if (vt && vt->is_array()) {
                int timestep = (current_timestep_ >= 0) ? current_timestep_ : 0;
                // Arity is the number of indices THIS read consumed, which may be
                // fewer than the array's nesting depth (a partial read yields an
                // array-valued result) — hence the explicit arity argument.
                const z3::func_decl& fn =
                    uf_for(fl, vt, timestep, static_cast<int>(idx_ids.size()));
                z3::expr_vector args(ctx_);
                for (ExprID ix : idx_ids) args.push_back(convert_node(ix));
                return fn(args);
            }
        }
        return std::nullopt;
    }

    // Whole-array/set EQUALS under UF mode: there is no Array-theory extensionality to
    // compare two array/set *values* with a single Z3 `==`, so expand into a conjunction
    // of pointwise equality over the enumerated static domain instead. Theory mode is
    // unaffected — nullopt falls through to the generic EQUALS handling (a single Z3
    // array `==`, see docs/z3_encoding_rationale.md §4, which Z3 resolves via
    // extensionality).
    if (op == ExprOperator::EQUALS && pool.argument_count(id) == 2 &&
        variable_factory_->uf_mode()) {
        ExprID lhs = pool.argument(id, 0);
        ExprID rhs = pool.argument(id, 1);
        const Type* lt = value_type_of_expr(lhs, pool, problem_);
        if (lt && (lt->is_array() || lt->is_set())) {
            int timestep = (current_timestep_ >= 0) ? current_timestep_ : 0;
            auto domain = enumerate_array_domain(lt, *problem_);
            z3::expr_vector conjuncts(ctx_);
            for (const auto& cell : domain) {
                conjuncts.push_back(convert_array_cell_value(lhs, cell, timestep)
                                  == convert_array_cell_value(rhs, cell, timestep));
            }
            return conjuncts.empty() ? ctx_.bool_val(true) : z3::mk_and(conjuncts);
        }
    }

    return std::nullopt;
}

z3::expr GroundedEncodingVisitor::convert_node(ExprID id) {
    const ExprPool& pool = problem_->pool();
    const ExprNode& node = pool.get(id);
    auto kind = static_cast<ExprKind>(node.kind);

    // ---- Leaf nodes (no children) ----
    if (node.children.empty()) {
        if (std::holds_alternative<std::string>(node.payload)) {
            const std::string& symbol = std::get<std::string>(node.payload);
            const Type* type = nullptr;
            if (node.type_id >= 0 && node.type_id < static_cast<int>(problem_->types().size())) {
                type = &problem_->types()[node.type_id];
            }
            // [XTS] Domain objects (city0, robot1 …) are encoded as their global integer
            // index so comparisons like (= (loc ?p) city0) become int==int in Z3 —
            // no uninterpreted constants in the SMT encoding.
            // Non-objects (numeric constants defined in the domain, or quantifier variables
            // already substituted to ints) fall through to create_symbol_variable.
            if (problem_) {
                int idx = problem_->object_constant_index(id);
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

        // fluent_def is the schema (return type + parameter types) for this SV family.
        // It's needed to know how to build grounded_params below and which Z3 sort to use.
        // Linear scan is fine — fluents() is small (tens to low hundreds after grounding).
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

        // Build grounded parameters from argument children.
        // Result: e.g. at(robot, cityA) → grounded_params = [("robot", ObjType), ("cityA", ObjType)]
        //         → variable_factory_ gives Z3 bool const "at_robot_cityA_t"
        // [XTS] Function composition — a fluent argument that is itself a
        // STATE_VARIABLE, e.g. (connected (location ?p) ?q) — is not accepted.
        // _check_no_nested_fluents() (rantanplan/planner_wrapper.py) rejects it
        // before the problem reaches the backend, so such an argument falls
        // through to the throw below. The encoder used to expand it into an ITE
        // chain over all objects of the argument's type; that path was unsound
        // and is archived in xts/docs/attic/expand_object_sv_arg.cpp.

        std::vector<Parameter> grounded_params;
        grounded_params.reserve(pool.argument_count(id));
        size_t arg_index = 0;
        for (ExprID arg_id : pool.arguments(id)) {
            const ExprNode& arg = pool.get(arg_id);
            std::string param_name;
            if (std::holds_alternative<std::string>(arg.payload)) {
                param_name = std::get<std::string>(arg.payload);
            } else if (std::holds_alternative<int64_t>(arg.payload)) {
                int64_t int_val = std::get<int64_t>(arg.payload);
                // Object-typed parameter given as an out-of-bounds integer index:
                // the RPG / achievers analysis passes -1 (or ≥N) as a sentinel meaning
                // "no object / undefined". Return a neutral value so downstream
                // constraints stay satisfiable.
                // E.g. adjacent(robot, -1) where -1 means "no neighbour" → false.
                const Type* ptype = (arg_index < fluent_def->parameters().size())
                    ? fluent_def->parameters()[arg_index].type() : nullptr;
                if (ptype && ptype->is_object()) {
                    int64_t n_objs = static_cast<int64_t>(problem_->objects().size());
                    if (int_val < 0 || int_val >= n_objs) {
                        return fluent_def->is_predicate()
                            ? ctx_.bool_val(false)
                            : variable_factory_->make_numeric_val(static_cast<int64_t>(-1));
                    }
                    // Valid object index: look up the object's name.
                    param_name = problem_->object(static_cast<int>(int_val)).name();
                } else {
                    // Integer-typed fluent parameter (bounded-int domain).
                    param_name = std::to_string(int_val);
                }
            } else {
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
            grounded_params.emplace_back(param_name, param_type);
            ++arg_index;
        }

        Fluent grounded_fluent(fluent_def->name(), fluent_def->value_type(), grounded_params);
        int timestep = (current_timestep_ >= 0) ? current_timestep_ : 0;
        return variable_factory_->get_fluent_variable(grounded_fluent, timestep);
    }

    // ---- Function application ----
    if (kind == ExprKind::FUNCTION_APPLICATION) {
        auto op = static_cast<ExprOperator>(node.op);

        // [XTS] Quantifier expansion: forall/exists arrive as op=UNKNOWN with head
        // symbol "up:forall" / "up:exists".
        if (op == ExprOperator::UNKNOWN) return convert_quantifier(id);

        // [XTS] Set operators that expand over the finite element domain.
        if (auto handled = try_convert_set_op(id)) return *handled;

        // [XTS-UnFun] UF-mode interceptions for ARRAY_READ and whole-array/set EQUALS.
        // Both return nullopt in Theory mode, falling through to the switch below.
        if (auto handled = try_convert_uf_array_op(id)) return *handled;

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
            case ExprOperator::COUNT:         return handle_count(z3_args);

            // [XTS] ARRAY_READ: one indexing level → Z3 select(arr, idx). N-D reads chain as select(select(arr,i),j,...).
            case ExprOperator::ARRAY_READ:
                if (z3_args.size() != 2)
                    throw std::runtime_error("ARRAY_READ expects 2 arguments");
                return z3::select(z3_args[0], z3_args[1]);

            // [XTS] SET_CONSTANT: start from an all-false base array, then store true at each listed element's index.
            case ExprOperator::SET_CONSTANT: {
                z3::expr result = z3::const_array(ctx_.int_sort(), ctx_.bool_val(false));
                for (const z3::expr& e : z3_args)
                    result = z3::store(result, e, ctx_.bool_val(true));
                return result;
            }

            // [XTS] Set algebra: enumerate the finite element domain and compute each output cell as an
            // ITE — avoids ∀ quantifiers and keeps the encoding in QF_AX.
            case ExprOperator::SET_UNION:
            case ExprOperator::SET_INTERSECT:
            case ExprOperator::SET_DIFFERENCE: {
                if (z3_args.size() != 2)
                    throw std::runtime_error("SET_UNION/INTERSECT/DIFFERENCE expects 2 arguments");
                const Type* elem_type = elem_type_of_set_expr(pool.argument(id, 0));
                z3::expr result = z3::const_array(ctx_.int_sort(), ctx_.bool_val(false));
                auto build_cell = [&](const z3::expr& key) {
                    z3::expr a = z3::select(z3_args[0], key);
                    z3::expr b = z3::select(z3_args[1], key);
                    z3::expr val = (op == ExprOperator::SET_UNION)      ? (a || b)
                                 : (op == ExprOperator::SET_INTERSECT)  ? (a && b)
                                 : /* SET_DIFFERENCE */                   (a && !b);
                    result = z3::store(result, key, val);
                };
                // One store() per candidate element; an unresolved elem_type leaves the
                // all-false base array, exactly as before.
                for_each_set_element(elem_type, build_cell);
                return result;
            }

            // [XTS] ARRAY_CONSTANT: encodes an array literal with elements at consecutive indices.
            //   board = [0,0,1,0]  →  store(store(store(store(⊥, 0,0), 1,0), 2,1), 3,0)
            // For N-D arrays (board : array[4, array[4, int]]), z3_args[0] already has sort
            // Array(Int,Int), so make_default(Array(Int,Int)) = const_array(Int, int_val(0)).
            case ExprOperator::ARRAY_CONSTANT: {
                if (z3_args.empty())
                    return z3::const_array(ctx_.int_sort(), ctx_.int_val(0));
                // make_default: produce the Z3 "zero" constant for any sort depth.
                // bool → false; array(D,R) → const_array(D, zero(R)); int/real → 0.
                std::function<z3::expr(z3::sort)> make_default = [&](z3::sort s) -> z3::expr {
                    if (s.is_bool()) return ctx_.bool_val(false);
                    if (s.is_array())
                        return z3::const_array(s.array_domain(), make_default(s.array_range()));
                    return ctx_.int_val(0);
                };
                z3::expr result = z3::const_array(ctx_.int_sort(), make_default(z3_args[0].get_sort()));
                for (size_t i = 0; i < z3_args.size(); ++i)
                    result = z3::store(result, ctx_.int_val(static_cast<int>(i)), z3_args[i]);
                return result;
            }

            default:
                throw std::runtime_error("Unsupported operator in ExprID conversion: " + std::to_string(static_cast<int>(op)));
        }
    }

    throw std::runtime_error("Unhandled ExprNode kind in convert_node: " + std::to_string(node.kind));
}

// ============================================================================
// convert_membership: "elem ∈ set_expr" → Z3 boolean
// Recurses structurally into compound set expressions so we never need an
// intermediate Z3 array for unions/intersections/differences used in conditions.
// ============================================================================
z3::expr GroundedEncodingVisitor::convert_membership(const z3::expr& elem, ExprID set_id) {
    const ExprPool& pool = problem_->pool();

    if (pool.is_state_variable(set_id)) {
        // [XTS-UnFun] Top-level set fluent under UF mode: apply its characteristic
        // function directly instead of materializing an Array(Int,Bool) Theory value
        // and selecting into it. (A set nested inside an array cell is read via
        // convert_node(set_id) below, which already resolves ARRAY_READ chains through
        // the UF path when applicable — see convert_node's ARRAY_READ interception —
        // and returns a genuine Array(Int,Bool) leaf value there by design; only a
        // bare top-level set fluent gets its own UF function.)
        if (variable_factory_->uf_mode()) {
            Fluent fl = build_grounded_fluent(set_id);
            const Type* vt = fl.value_type();
            if (vt && vt->is_set()) {
                int timestep = (current_timestep_ >= 0) ? current_timestep_ : 0;
                // arity 1; the element sort resolves to Bool for a set type (see
                // resolve_elem_sort_at_depth's is_set() guard at depth <= 1).
                return uf_for(fl, vt, timestep, 1)(elem);
            }
        }
        return z3::select(convert_node(set_id), elem);
    }

    if (pool.is_function_application(set_id)) {
        ExprOperator op = pool.op(set_id);

        if (op == ExprOperator::SET_CONSTANT) {
            // elem == a || elem == b || elem == c
            z3::expr_vector clauses(ctx_);
            for (size_t i = 0; i < pool.argument_count(set_id); ++i)
                clauses.push_back(elem == convert_node(pool.argument(set_id, i)));
            return clauses.empty() ? ctx_.bool_val(false) : z3::mk_or(clauses);
        }
        if (op == ExprOperator::SET_UNION && pool.argument_count(set_id) == 2) {
            return convert_membership(elem, pool.argument(set_id, 0))
                || convert_membership(elem, pool.argument(set_id, 1));
        }
        if (op == ExprOperator::SET_INTERSECT && pool.argument_count(set_id) == 2) {
            return convert_membership(elem, pool.argument(set_id, 0))
                && convert_membership(elem, pool.argument(set_id, 1));
        }
        if (op == ExprOperator::SET_DIFFERENCE && pool.argument_count(set_id) == 2) {
            return convert_membership(elem, pool.argument(set_id, 0))
                && !convert_membership(elem, pool.argument(set_id, 1));
        }
        if (op == ExprOperator::ARRAY_READ) {
            // ARRAY_READ(arr, idx) returns a set-valued element; select elem from it.
            return z3::select(convert_node(set_id), elem);
        }
    }
    throw std::runtime_error("Unsupported set expression in convert_membership");
}

// ============================================================================
// elem_type_of_set_expr: return element Type* for a set-valued ExprID
// ============================================================================
// Helper: given a fluent's value Type*, extract the element type of the set it
// represents (or the set its array elements represent).  Returns nullptr if the
// type is not a set or array-of-set.
static const Type* set_elem_type_from_fluent_type(const Type* vt, const Problem* problem) {
    if (!vt) return nullptr;
    if (vt->is_set()) {
        const std::string& en = vt->set_element_type_name();
        const Type* t = detail::find_element_type(en, *problem);
        return t;
    }
    if (vt->is_array()) {
        const std::string& aet_name = vt->array_element_type_name();
        const Type* aet = detail::find_element_type(aet_name, *problem);
        return set_elem_type_from_fluent_type(aet, problem);  // recurse for arrays-of-sets
    }
    return nullptr;
}

// Given a STATE_VARIABLE ExprID, return the fluent's value type via the name lookup.
static const Type* fluent_value_type(ExprID sv_id, const ExprPool& pool, const Problem* problem) {
    if (!pool.is_state_variable(sv_id)) return nullptr;
    ExprID sym = pool.head_symbol_id(sv_id);
    if (!pool.payload_is_string(sym)) return nullptr;
    const Fluent* f = problem->find_fluent(pool.payload_string(sym));
    return f ? f->value_type() : nullptr;
}

// Return the value type of any expression that may be a STATE_VARIABLE, ARRAY_READ, or ARRAY_CONSTANT.
// For ARRAY_READ(inner, i): recurse to get inner's array type, then return its element type.
// For ARRAY_CONSTANT / any typed node: use type_id directly.
static const Type* value_type_of_expr(ExprID eid, const ExprPool& pool, const Problem* problem) {
    if (pool.is_state_variable(eid))
        return fluent_value_type(eid, pool, problem);
    if (pool.is_function_application(eid)) {
        ExprOperator op = pool.op(eid);
        if (op == ExprOperator::ARRAY_READ && pool.argument_count(eid) >= 1) {
            const Type* inner_type = value_type_of_expr(pool.argument(eid, 0), pool, problem);
            if (!inner_type || !inner_type->is_array()) return nullptr;
            const std::string& ename = inner_type->array_element_type_name();
            const Type* et = detail::find_element_type(ename, *problem);
            return et;
        }
        // For ARRAY_CONSTANT and other typed function applications, fall through to type_id.
    }
    // Last resort: use the type_id recorded on the node itself.
    int tid = pool.type_id(eid);
    if (tid >= 0 && static_cast<size_t>(tid) < problem->types().size())
        return &problem->types()[tid];
    return nullptr;
}

// Count the number of nested ARRAY_READ wrappers and return the innermost node.
static int count_array_read_depth(ExprID set_id, const ExprPool& pool, ExprID* innermost) {
    int depth = 0;
    ExprID cur = set_id;
    while (pool.is_function_application(cur) &&
           pool.op(cur) == ExprOperator::ARRAY_READ &&
           pool.argument_count(cur) >= 1) {
        ++depth;
        cur = pool.argument(cur, 0);
    }
    if (innermost) *innermost = cur;
    return depth;
}

const Type* GroundedEncodingVisitor::elem_type_of_set_expr(ExprID set_id) const {
    const ExprPool& pool = problem_->pool();

    // Direct set-typed SV
    if (pool.is_state_variable(set_id))
        return set_elem_type_from_fluent_type(fluent_value_type(set_id, pool, problem_), problem_);

    if (pool.is_function_application(set_id)) {
        ExprOperator op = pool.op(set_id);

        // SET_CONSTANT literal: four fallbacks in priority order.
        //
        // 0. Own type_id: works when the type_id is concrete (e.g. a set-typed SV that
        //    was directly substituted).
        //
        // 1. First element's type_id: object or bounded-int constants carry their type
        //    directly in the ExprPool.
        //
        // 2. Reverse the static substitution: find the initial-state assignment whose
        //    value_id equals this ExprID, then read the element type from the fluent's
        //    declared type.
        //
        // 3. [XTS] Scan declared types: handles SET_CONSTANTs from ARRAY_READ folding
        //    of a static N-D array-of-sets.  The proto writer doesn't annotate nested
        //    literal elements so type_ids are -1, but the canonical set type still
        //    appears in problem_->types().  If exactly one set element type exists
        //    across all declared set types, it must be the right one.
        if (op == ExprOperator::SET_CONSTANT) {
            // Fallback 0: own type_id
            {
                const Type* own_type = value_type_of_expr(set_id, pool, problem_);
                const Type* et = set_elem_type_from_fluent_type(own_type, problem_);
                if (et) return et;
            }
            // Fallback 1: element's concrete type (object or bounded-int)
            if (pool.argument_count(set_id) > 0) {
                ExprID first_arg = pool.argument(set_id, 0);
                int tid = pool.type_id(first_arg);
                if (tid >= 0 && static_cast<size_t>(tid) < problem_->types().size()) {
                    const Type* arg_type = &problem_->types()[tid];
                    if (arg_type->is_object() || arg_type->is_bounded_int()) return arg_type;
                }
            }
            // Fallback 2: reverse the static substitution via initial-state scan
            for (const auto& assignment : problem_->initial_state()) {
                if (assignment.value_id() == set_id) {
                    const Type* fvt = problem_->type_for_id(assignment.fluent_id());
                    const Type* et = set_elem_type_from_fluent_type(fvt, problem_);
                    if (et) return et;
                }
            }
            // [XTS] Fallback 3: scan all declared types for is_set() types.
            {
                const Type* found_elem = nullptr;
                bool ambiguous = false;
                for (const auto& t : problem_->types()) {
                    if (!t.is_set()) continue;
                    const Type* et = set_elem_type_from_fluent_type(&t, problem_);
                    if (!et) continue;
                    if (!found_elem) {
                        found_elem = et;
                    } else if (found_elem != et) {
                        ambiguous = true;
                        break;
                    }
                }
                if (found_elem && !ambiguous) return found_elem;
            }
            return nullptr;
        }

        if (pool.argument_count(set_id) < 1) return nullptr;

        // ARRAY_READ(arr, idx): element type = (element type of array's value type).
        // arr may itself be an ARRAY_READ (2D case), so use value_type_of_expr to walk the chain.
        if (op == ExprOperator::ARRAY_READ) {
            ExprID arr_id = pool.argument(set_id, 0);
            const Type* arr_vt = value_type_of_expr(arr_id, pool, problem_);
            const Type* result = set_elem_type_from_fluent_type(arr_vt, problem_);
            if (result) return result;

            // Fallback: nodes from static-fluent substitution (ARRAY_CONSTANT) carry no
            // type_id.  Search all problem fluents for one whose value type, after following
            // `depth` array nestings, produces a set.  This handles the case where a static
            // array fluent was replaced by an ARRAY_CONSTANT literal.
            int depth = count_array_read_depth(set_id, pool, nullptr);
            for (const Fluent& f : problem_->fluents()) {
                const Type* vt = f.value_type();
                int d = depth;
                while (d > 0 && vt && vt->is_array()) {
                    const std::string& ename = vt->array_element_type_name();
                    vt = detail::find_element_type(ename, *problem_);
                    --d;
                }
                if (d == 0 && vt && vt->is_set()) {
                    const Type* elem = set_elem_type_from_fluent_type(vt, problem_);
                    if (elem) return elem;
                }
            }
            return nullptr;
        }

        // Binary set ops and other compound: recurse into first operand.
        return elem_type_of_set_expr(pool.argument(set_id, 0));
    }

    return nullptr;
}

// ============================================================================
// [XTS-UnFun] UF encoding support: peeling reads, building fluent identities,
// and evaluating array/set-valued expressions pointwise.
// ============================================================================

ExprID GroundedEncodingVisitor::peel_array_read_chain(ExprID id, const ExprPool& pool,
                                                       std::vector<ExprID>& indices_out) {
    std::vector<ExprID> rev;
    ExprID cur = id;
    while (pool.is_function_application(cur) &&
           pool.op(cur) == ExprOperator::ARRAY_READ &&
           pool.argument_count(cur) >= 2) {
        rev.push_back(pool.argument(cur, 1));
        cur = pool.argument(cur, 0);
    }
    indices_out.assign(rev.rbegin(), rev.rend());
    return cur;
}

// [XTS-UnFun] Independent of convert_node's generic STATE_VARIABLE branch (which is
// left untouched to avoid disturbing its out-of-bounds object-index sentinel
// short-circuit — see the comment there). This assumes `sv_id` is a fully concrete
// array/set base fluent identity, which is always true for the callers that use it
// (peeled ARRAY_READ roots, array_epc_index_ keys, EQUALS operands) — array/set base
// fluents are never passed a nested-SV or out-of-range sentinel argument in practice.
Fluent GroundedEncodingVisitor::build_grounded_fluent(ExprID sv_id) const {
    const ExprPool& pool = problem_->pool();
    if (!pool.is_state_variable(sv_id))
        throw std::runtime_error("build_grounded_fluent: expected a STATE_VARIABLE ExprID");

    const ExprNode& fluent_sym = pool.head_symbol_node(sv_id);
    if (!std::holds_alternative<std::string>(fluent_sym.payload))
        throw std::runtime_error("build_grounded_fluent: STATE_VARIABLE head is not a symbol");
    const std::string& fluent_name = std::get<std::string>(fluent_sym.payload);

    const Fluent* fluent_def = nullptr;
    for (const auto& fluent : problem_->fluents()) {
        if (fluent.name() == fluent_name) { fluent_def = &fluent; break; }
    }
    if (!fluent_def)
        throw std::runtime_error("build_grounded_fluent: fluent definition not found for '" + fluent_name + "'");

    std::vector<Parameter> grounded_params;
    grounded_params.reserve(pool.argument_count(sv_id));
    size_t arg_index = 0;
    for (ExprID arg_id : pool.arguments(sv_id)) {
        const ExprNode& arg = pool.get(arg_id);
        std::string param_name;
        if (std::holds_alternative<std::string>(arg.payload)) {
            param_name = std::get<std::string>(arg.payload);
        } else if (std::holds_alternative<int64_t>(arg.payload)) {
            int64_t int_val = std::get<int64_t>(arg.payload);
            const Type* ptype = (arg_index < fluent_def->parameters().size())
                ? fluent_def->parameters()[arg_index].type() : nullptr;
            if (ptype && ptype->is_object()) {
                int64_t n_objs = static_cast<int64_t>(problem_->objects().size());
                if (int_val < 0 || int_val >= n_objs)
                    throw std::runtime_error(
                        "build_grounded_fluent: out-of-range object index in array/set "
                        "base fluent '" + fluent_name + "' (unexpected: base array/set "
                        "identities are always fully concrete)");
                param_name = problem_->object(static_cast<int>(int_val)).name();
            } else {
                param_name = std::to_string(int_val);
            }
        } else {
            throw std::runtime_error(
                "build_grounded_fluent: non-constant argument in fluent '" + fluent_name + "'");
        }
        const Type* param_type = (arg_index < fluent_def->parameters().size())
            ? fluent_def->parameters()[arg_index].type() : nullptr;
        if (!param_type) param_type = problem_->find_type("object");
        grounded_params.emplace_back(param_name, param_type);
        ++arg_index;
    }

    return Fluent(fluent_def->name(), fluent_def->value_type(), grounded_params);
}

// [XTS-UnFun] See header for the contract and worked examples.
const z3::func_decl& GroundedEncodingVisitor::uf_for(const Fluent& fl, const Type* vt,
                                                      int timestep, int arity) {
    // arity < 0 means "derive the shape from the type"; otherwise the caller's index
    // count wins and only the element sort is looked up (at that depth).
    // canon is the type's own nesting depth. get_array_uf needs it to tell a
    // whole-fluent function from a partial read of the same fluent at the same
    // timestep -- different functions that used to collide in its cache.
    const auto canon = variable_factory_->resolve_uf_shape(vt);
    auto [n, elem_sort] = (arity < 0)
        ? canon
        : std::pair<unsigned, z3::sort>{static_cast<unsigned>(arity),
                                        variable_factory_->resolve_elem_sort_at_depth(vt, arity)};
    return variable_factory_->get_array_uf(fl, timestep, n, elem_sort, canon.first);
}

// [XTS-UnFun] Convenience overload for callers that don't already hold the Fluent.
const z3::func_decl& GroundedEncodingVisitor::uf_for(ExprID sv_id, const Type* vt,
                                                      int timestep, int arity) {
    return uf_for(build_grounded_fluent(sv_id), vt, timestep, arity);
}

// [XTS-UnFun] See header for the contract.
void GroundedEncodingVisitor::for_each_set_element(
        const Type* elem_type, const std::function<void(const z3::expr&)>& fn) const {
    for (int64_t v : enumerate_set_element_domain(elem_type, *problem_)) {
        // make_numeric_val (not ctx_.int_val) so the key's sort matches however the rest
        // of this problem's numerics are encoded — object indices included.
        fn(variable_factory_->make_numeric_val(v));
    }
}

// [XTS-UnFun] See header for the contract. Handles, in order:
//   1. Another array/set fluent (STATE_VARIABLE) — apply its UF function (UF mode)
//      or nested-select its materialized Theory value (Theory mode / non-UF fallback).
//   2. Set-shaped compound expressions (SET_CONSTANT/UNION/INTERSECT/DIFFERENCE) —
//      delegate to convert_membership, which already evaluates these structurally
//      without materializing an intermediate array, in either mode.
//   3. ARRAY_CONSTANT literals — index directly into the literal's children,
//      recursing for nested (N-D) literals.
//   4. Fallback — materialize via convert_node and nested-select. Only reached for
//      expression shapes not covered above (e.g. an ARRAY_READ sub-array copy used as
//      a whole-array assign RHS); uses Array theory locally for this one transient
//      term, which is fine since it is never a persistent fluent's own storage.
z3::expr GroundedEncodingVisitor::convert_array_cell_value(ExprID val_id, const std::vector<int64_t>& cell,
                                                            int timestep) {
    // [XTS-UnFun] This method is called directly by GroundedEncoder::encode_frames,
    // NOT through convert_from_pool — so current_timestep_ is whatever was left over
    // from some unrelated earlier call, not `timestep`. convert_membership and
    // convert_node (called below, and transitively by build_grounded_fluent's callers)
    // both read current_timestep_ rather than taking a timestep parameter, so it must
    // be set here explicitly or every UF set-fluent lookup silently resolves to the
    // wrong timestep's function. Mirrors convert_from_pool's save/restore.
    int saved_timestep = current_timestep_;
    current_timestep_ = timestep;
    struct RestoreTimestep {
        int& slot;
        int saved;
        ~RestoreTimestep() { slot = saved; }
    } restore_guard{current_timestep_, saved_timestep};

    const ExprPool& pool = problem_->pool();

    if (pool.is_state_variable(val_id)) {
        Fluent fl = build_grounded_fluent(val_id);
        const Type* vt = fl.value_type();
        if (vt && vt->is_set()) {
            return convert_membership(variable_factory_->make_numeric_val(cell.at(0)), val_id);
        }
        if (vt && vt->is_array() && variable_factory_->uf_mode()) {
            // Arity is the length of the cell tuple being evaluated.
            const z3::func_decl& fn = uf_for(fl, vt, timestep, static_cast<int>(cell.size()));
            return fn(variable_factory_->cell_args(cell));
        }
        // Theory mode (or non-array/set — shouldn't happen for a real caller, but stay
        // safe): fall through to the shared materialize-then-select fallback below.
    }

    if (pool.is_function_application(val_id)) {
        ExprOperator op = pool.op(val_id);
        if ((op == ExprOperator::SET_CONSTANT || op == ExprOperator::SET_UNION ||
             op == ExprOperator::SET_INTERSECT || op == ExprOperator::SET_DIFFERENCE) &&
            cell.size() == 1) {
            return convert_membership(variable_factory_->make_numeric_val(cell.at(0)), val_id);
        }
        if (op == ExprOperator::ARRAY_CONSTANT && !cell.empty()) {
            int64_t idx = cell.at(0);
            if (idx < 0 || static_cast<size_t>(idx) >= pool.argument_count(val_id))
                throw std::runtime_error("convert_array_cell_value: ARRAY_CONSTANT index out of range");
            ExprID child = pool.argument(val_id, static_cast<size_t>(idx));
            if (cell.size() == 1) return convert_node(child);
            std::vector<int64_t> rest(cell.begin() + 1, cell.end());
            return convert_array_cell_value(child, rest, timestep);
        }
    }

    // Fallback: materialize (Theory-style) then nested-select.
    z3::expr cur = convert_node(val_id);
    for (int64_t c : cell) cur = z3::select(cur, ctx_.int_val(c));
    return cur;
}

} // namespace rantanplan
