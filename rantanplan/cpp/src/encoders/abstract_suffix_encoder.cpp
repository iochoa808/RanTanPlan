#include "abstract_suffix_encoder.hpp"
#include "base_encoder.hpp"
#include "grounded_encoder.hpp"
#include "../analysis/graph.hpp"
#include "../analysis/interference_analysis.hpp"
#include "../problem/visitors/fluent_collector.hpp"
#include "../problem/expr_enums.hpp"

namespace rantanplan {

// ============================================================================
// Construction
// ============================================================================

AbstractSuffixEncoder::AbstractSuffixEncoder(
    const Problem& problem, z3::context& ctx,
    BaseEncoder& encoder,
    const InterferenceAnalysis* interference,
    SemanticsKind semantics)
    : problem_(problem), ctx_(ctx), encoder_(encoder),
      interference_(interference), semantics_(semantics),
      cached_loop_formulas_(ctx.bool_val(true)),
      epc_index_(nullptr) {

    // Access epc_index from the concrete encoder (all encoders derive from GroundedEncoder)
    auto* grounded = dynamic_cast<GroundedEncoder*>(&encoder_);
    if (grounded) {
        epc_index_ = &grounded->get_epc_index();
    }
}

void AbstractSuffixEncoder::initialize() {
    const auto& actions = problem_.actions();
    const auto& fluents = problem_.grounded_fluents();

    // One abstract Bool per action
    action_abs_.reserve(actions.size());
    for (const auto& action : actions) {
        std::string name = "abs_" + action.name();
        action_abs_.push_back(ctx_.bool_const(name.c_str()));
    }

    // One mod_v Bool per grounded fluent
    mod_vars_.reserve(fluents.size());
    for (size_t i = 0; i < fluents.size(); ++i) {
        std::string name = "mod_" + std::to_string(i);
        mod_vars_.push_back(ctx_.bool_const(name.c_str()));
        fluent_to_mod_index_[fluents[i]] = i;
    }
}

// ============================================================================
// Helpers
// ============================================================================

std::vector<ExprID> AbstractSuffixEncoder::get_precondition_conjuncts(ExprID precond_id) const {
    std::vector<ExprID> conjuncts;
    const auto& pool = problem_.pool();

    if (!precond_id.valid()) return conjuncts;

    if (pool.is_and(precond_id)) {
        // AND node — arguments (children[1..]) are the conjuncts.
        // children[0] is the AND function symbol.
        for (ExprID arg : pool.arguments(precond_id)) {
            conjuncts.push_back(arg);
        }
    } else {
        // Single conjunct
        conjuncts.push_back(precond_id);
    }
    return conjuncts;
}

z3::expr_vector AbstractSuffixEncoder::get_mod_vars_for_expr(ExprID expr_id) const {
    z3::expr_vector mods(ctx_);
    FluentCollector collector(problem_);
    collector.collect_from_id(expr_id);
    for (ExprID fluent_id : collector.get_fluents()) {
        auto it = fluent_to_mod_index_.find(fluent_id);
        if (it != fluent_to_mod_index_.end()) {
            mods.push_back(mod_vars_[it->second]);
        }
    }
    return mods;
}

// ============================================================================
// Permanent: mod_v equivalences (Eq. 4B)
//
// For each grounded fluent v: mod_v <=> (OR of a^abs for actions affecting v).
// Uses the epc_index_ (effects-per-condition index) from GroundedEncoder.
// Horizon-independent — only references abstract vars and action structure.
// ============================================================================

z3::expr AbstractSuffixEncoder::encode_mod_v_equivalences() {
    z3::expr_vector all_constraints(ctx_);

    if (epc_index_) {
        for (size_t v_idx = 0; v_idx < mod_vars_.size(); ++v_idx) {
            ExprID fluent_id = problem_.grounded_fluents()[v_idx];
            auto it = epc_index_->find(fluent_id);

            z3::expr_vector affecting_abs(ctx_);
            if (it != epc_index_->end()) {
                for (const auto& [action_ptr, _] : it->second) {
                    int aid = action_ptr->id();
                    if (aid >= 0 && static_cast<size_t>(aid) < action_abs_.size()) {
                        affecting_abs.push_back(action_abs_[aid]);
                    }
                }
            }

            if (affecting_abs.empty()) {
                // No action can affect this fluent — mod_v must be false
                all_constraints.push_back(!mod_vars_[v_idx]);
            } else {
                all_constraints.push_back(
                    mod_vars_[v_idx] == z3::mk_or(affecting_abs));
            }
        }
    }

    return all_constraints.empty() ? ctx_.bool_val(true) : z3::mk_and(all_constraints);
}

// ============================================================================
// Permanent: Loop Formulas (Eq. 5)
//
// Prevent circular support among abstract actions. Without these, the abstract
// suffix could contain a cycle where action A's precondition is "justified" by
// mod_v from action B, and B's precondition is justified by mod_w from A —
// neither action has genuine external support, yet both appear feasible.
//
// Implementation:
//   1. Build a dependency graph with two kinds of nodes:
//      - mod_v nodes (one per grounded fluent, indices 0..num_fluents-1)
//      - precondition atom nodes (indices num_fluents..)
//      Edges capture "action a affects fluent v AND requires precondition phi"
//      and "precondition phi references fluent v".
//   2. Compute SCCs using Tarjan's algorithm (via existing Graph class).
//   3. For each non-trivial SCC: require that if any mod_v in the SCC is true,
//      there must be external support (a mod_v outside the SCC with an edge in).
//      If no external support exists, all mod_v nodes in the SCC must be false.
//
// The result is cached since it depends only on action structure, not horizon.
// ============================================================================

z3::expr AbstractSuffixEncoder::encode_loop_formulas() {
    if (loop_formulas_built_) return cached_loop_formulas_;

    // Build dependency graph for loop formula detection.
    // Nodes: mod_v variables (indices 0..num_fluents-1)
    //        + precondition atoms (indices num_fluents..)
    // Edges: for each action a^abs, for each mod_v in effects of a,
    //        add edge from mod_v node to each precondition atom node of a.

    const auto& actions = problem_.actions();
    const auto& fluents = problem_.grounded_fluents();
    size_t num_fluents = fluents.size();

    // Map precondition atom ExprIDs to node indices
    std::unordered_map<ExprID, size_t> precond_to_node;
    std::vector<ExprID> node_to_precond;  // for reverse lookup

    // Assign node indices: [0..num_fluents-1] for mod_v, [num_fluents..] for precond atoms
    size_t next_node = num_fluents;

    for (const auto& action : actions) {
        if (!action.has_precondition()) continue;
        auto conjuncts = get_precondition_conjuncts(action.precondition_id());
        for (ExprID phi : conjuncts) {
            if (precond_to_node.find(phi) == precond_to_node.end()) {
                precond_to_node[phi] = next_node++;
                node_to_precond.push_back(phi);
            }
        }
    }

    size_t total_nodes = next_node;
    Graph graph;
    for (size_t i = 0; i < total_nodes; ++i) {
        graph.add_node();
    }

    // Build edges: for each action, for each fluent v it affects,
    // add edge from mod_v -> each precondition atom of the action
    for (size_t a_idx = 0; a_idx < actions.size(); ++a_idx) {
        const Action& action = actions[a_idx];
        if (!action.has_precondition()) continue;

        auto conjuncts = get_precondition_conjuncts(action.precondition_id());

        // Find fluents affected by this action (from epc_index, look for this action)
        for (const auto& effect : action.effects()) {
            ExprID fluent_id = effect.effect_expression().fluent_id();
            auto it = fluent_to_mod_index_.find(fluent_id);
            if (it == fluent_to_mod_index_.end()) continue;
            size_t mod_node = it->second;

            for (ExprID phi : conjuncts) {
                auto pit = precond_to_node.find(phi);
                if (pit != precond_to_node.end()) {
                    graph.add_edge(mod_node, pit->second);
                }
            }
        }

        // Also: precondition atoms depend on mod_v for fluents they reference
        for (ExprID phi : conjuncts) {
            auto pit = precond_to_node.find(phi);
            if (pit == precond_to_node.end()) continue;

            FluentCollector collector(problem_);
            collector.collect_from_id(phi);
            for (ExprID fid : collector.get_fluents()) {
                auto mit = fluent_to_mod_index_.find(fid);
                if (mit != fluent_to_mod_index_.end()) {
                    graph.add_edge(pit->second, mit->second);
                }
            }
        }
    }

    auto sccs = graph.compute_strongly_connected_components();

    z3::expr_vector loop_constraints(ctx_);

    for (const auto& scc : sccs) {
        // Skip trivial SCCs (size 1, no self-loop)
        if (scc.size() <= 1) {
            if (scc.size() == 1 && !graph.has_edge(scc[0], scc[0])) {
                continue;
            }
        }

        std::unordered_set<int> scc_set(scc.begin(), scc.end());

        // Build OR of all nodes in SCC (as z3 expressions)
        z3::expr_vector scc_exprs(ctx_);
        for (int node : scc) {
            if (static_cast<size_t>(node) < num_fluents) {
                scc_exprs.push_back(mod_vars_[node]);
            }
            // Precondition atoms are not z3 variables in the abstract suffix;
            // they participate in the dependency graph but the loop formula
            // constraint only applies to mod_v variables.
        }

        if (scc_exprs.empty()) continue;

        // Find R(L): nodes outside SCC with edges into SCC
        z3::expr_vector external_support(ctx_);
        for (int node : scc) {
            // Check all potential predecessors
            for (size_t pred = 0; pred < total_nodes; ++pred) {
                if (scc_set.count(pred)) continue;  // skip SCC-internal
                if (graph.has_edge(pred, node)) {
                    if (pred < num_fluents) {
                        external_support.push_back(mod_vars_[pred]);
                    }
                    // External precondition atoms: they represent concrete
                    // state conditions that can provide external support.
                    // Treat them as always satisfiable (grounded in the
                    // concrete prefix) by adding unconditional support.
                    external_support.push_back(ctx_.bool_val(true));
                }
            }
        }

        if (external_support.empty()) {
            // No external support — SCC nodes must all be false
            for (unsigned i = 0; i < scc_exprs.size(); ++i) {
                loop_constraints.push_back(!scc_exprs[i]);
            }
        } else {
            // (OR scc_nodes) => (OR external_support)
            loop_constraints.push_back(
                z3::implies(z3::mk_or(scc_exprs), z3::mk_or(external_support)));
        }
    }

    cached_loop_formulas_ = loop_constraints.empty()
        ? ctx_.bool_val(true)
        : z3::mk_and(loop_constraints);
    loop_formulas_built_ = true;

    return cached_loop_formulas_;
}

// ============================================================================
// Per-step incremental: Axiom 6 — no arbitrary deferral (Eq. 6)
//
// Prevents the solver from trivially deferring actions to later timesteps
// without justification. For each concrete action a at step i > 0, a can only
// be true if at least one of:
//   - a was already executing at step i-1 (persistence)
//   - some precondition of a was unsatisfied at i-1 (couldn't execute earlier)
//   - a mutex action a' was executing at i-1 (blocked by interference)
//
// The mutex set M_a depends on the semantics:
//   - Sequential: M_a = all other actions (only one action per step)
//   - Forall/Exists: M_a = actions with bidirectional interference
//
// Only references concrete action variables at steps i and i-1 — once added,
// remains valid forever.
// ============================================================================

z3::expr AbstractSuffixEncoder::encode_axiom_6(int i) {
    // For each action a, at step i > 0:
    // a_i => (a_{i-1} OR (OR_{phi in pre_a} NOT phi_{i-1}) OR (OR_{a' in M_a} a'_{i-1}))
    z3::expr_vector all_constraints(ctx_);
    const auto& actions = problem_.actions();
    auto& vf = encoder_.get_variable_factory();

    for (size_t a_idx = 0; a_idx < actions.size(); ++a_idx) {
        const Action& action = actions[a_idx];
        const z3::expr& a_i = vf.get_action_variable(action, i);
        const z3::expr& a_prev = vf.get_action_variable(action, i - 1);

        z3::expr_vector reasons(ctx_);
        reasons.push_back(a_prev);  // action was already executing

        // Precondition was not satisfied at i-1
        if (action.has_precondition()) {
            auto conjuncts = get_precondition_conjuncts(action.precondition_id());
            for (ExprID phi : conjuncts) {
                reasons.push_back(!encoder_.convert_expr_id_to_z3(phi, i - 1));
            }
        }

        // Mutex action was executing at i-1
        if (semantics_ == SemanticsKind::Sequential) {
            // M_a = all other actions
            for (size_t b_idx = 0; b_idx < actions.size(); ++b_idx) {
                if (b_idx == a_idx) continue;
                reasons.push_back(vf.get_action_variable(actions[b_idx], i - 1));
            }
        } else if (interference_) {
            // M_a = actions with bidirectional syntactic interference.
            // Syntactic interference is a sound overapproximation: it may include
            // pairs that don't interfere semantically, making axiom 6 slightly
            // weaker (more deferral reasons) but still a valid lower bound.
            // The semantic precision is handled by the propagator during solving.
            // TODO: Explore a propagator-based lazy approach where axiom 6 clauses
            // are generated on-demand only for actions the solver actually selects,
            // avoiding even the syntactic O(N^2) iteration for large action sets.
            for (size_t b_idx = 0; b_idx < actions.size(); ++b_idx) {
                if (b_idx == a_idx) continue;
                if (interference_->has_syntactic_interference(action, actions[b_idx]) ||
                    interference_->has_syntactic_interference(actions[b_idx], action)) {
                    reasons.push_back(vf.get_action_variable(actions[b_idx], i - 1));
                }
            }
        }

        all_constraints.push_back(z3::implies(a_i, z3::mk_or(reasons)));
    }

    return all_constraints.empty() ? ctx_.bool_val(true) : z3::mk_and(all_constraints);
}

// ============================================================================
// Per-step incremental: Axiom 7 step (Eq. 7, decomposed)
//
// Original Eq. 7: (OR a^abs) => AND_{i=0}^{n-1} (OR_a a_i)
// Decomposed into individual per-step clauses:
//   encode_axiom_7_step(i): (OR a^abs) => (OR_a a_i)
//
// The conjunction of all steps 0..n-1 is equivalent to the original.
// Each clause is independent and permanent once added.
// ============================================================================

z3::expr AbstractSuffixEncoder::encode_axiom_7_step(int i) {
    const auto& actions = problem_.actions();
    auto& vf = encoder_.get_variable_factory();

    if (action_abs_.empty()) return ctx_.bool_val(true);

    z3::expr_vector any_abstract(ctx_);
    for (const auto& a_abs : action_abs_) {
        any_abstract.push_back(a_abs);
    }

    z3::expr_vector any_concrete(ctx_);
    for (const auto& action : actions) {
        any_concrete.push_back(vf.get_action_variable(action, i));
    }

    if (any_concrete.empty()) return ctx_.bool_val(true);

    return z3::implies(z3::mk_or(any_abstract), z3::mk_or(any_concrete));
}

// ============================================================================
// Horizon-dependent: Relaxed Preconditions (Eq. 4A)
//
// For each action a: a^abs => AND_{phi in pre_a} (phi_n OR OR_{v in vars(phi)} mod_v)
//
// The phi_n term evaluates preconditions at state n, making this
// horizon-dependent. The planner gates this with an assumption literal.
// ============================================================================

z3::expr AbstractSuffixEncoder::encode_relaxed_preconditions(int n) {
    z3::expr_vector all_constraints(ctx_);
    const auto& actions = problem_.actions();

    for (size_t a_idx = 0; a_idx < actions.size(); ++a_idx) {
        const Action& action = actions[a_idx];
        if (!action.has_precondition()) continue;

        auto conjuncts = get_precondition_conjuncts(action.precondition_id());
        z3::expr_vector relaxed_preconds(ctx_);

        for (ExprID phi : conjuncts) {
            z3::expr phi_at_n = encoder_.convert_expr_id_to_z3(phi, n);
            z3::expr_vector mods = get_mod_vars_for_expr(phi);

            if (mods.empty()) {
                relaxed_preconds.push_back(phi_at_n);
            } else {
                z3::expr_vector disj(ctx_);
                disj.push_back(phi_at_n);
                for (unsigned i = 0; i < mods.size(); ++i) {
                    disj.push_back(mods[i]);
                }
                relaxed_preconds.push_back(z3::mk_or(disj));
            }
        }

        if (!relaxed_preconds.empty()) {
            all_constraints.push_back(
                z3::implies(action_abs_[a_idx], z3::mk_and(relaxed_preconds)));
        }
    }

    return all_constraints.empty() ? ctx_.bool_val(true) : z3::mk_and(all_constraints);
}

// ============================================================================
// Horizon-dependent: Abstract Goal (Eq. 3)
//
// Each goal g is relaxed: (g holds at state n) OR (some fluent in g could be
// modified by the abstract suffix). This allows the solver to find plans where
// goals are not yet achieved but could plausibly be achieved in the suffix.
//
// The g_at_n term evaluates goals at state n, making this horizon-dependent.
// The planner gates this with an assumption literal.
// ============================================================================

z3::expr AbstractSuffixEncoder::encode_abstract_goal(int n) {
    z3::expr_vector goal_constraints(ctx_);

    for (const auto& goal : problem_.goals()) {
        z3::expr g_at_n = encoder_.convert_expr_id_to_z3(goal.goal_id(), n);
        z3::expr_vector mods = get_mod_vars_for_expr(goal.goal_id());

        if (mods.empty()) {
            goal_constraints.push_back(g_at_n);
        } else {
            z3::expr_vector disj(ctx_);
            disj.push_back(g_at_n);
            for (unsigned i = 0; i < mods.size(); ++i) {
                disj.push_back(mods[i]);
            }
            goal_constraints.push_back(z3::mk_or(disj));
        }
    }

    return goal_constraints.empty() ? ctx_.bool_val(true) : z3::mk_and(goal_constraints);
}

// ============================================================================
// Cost and solution queries
// ============================================================================

z3::expr AbstractSuffixEncoder::build_total_cost(int n) {
    z3::expr total = ctx_.real_val(0);
    for (int i = 0; i < n; ++i) {
        total = total + build_concrete_cost_term(i);
    }
    total = total + build_abstract_cost_term();
    return total;
}

bool AbstractSuffixEncoder::is_concrete_solution(const z3::model& model) const {
    for (const auto& a_abs : action_abs_) {
        z3::expr val = model.eval(a_abs, true);
        if (val.is_true()) return false;
    }
    return true;
}

// ============================================================================
// Cost terms
//
// Concrete cost: for each action at each step, ITE(action_active, cost, 0).
// Every action has a cost_id (defaulting to 1 if no explicit cost was defined
// in the problem). For SDAC, cost evaluation uses virtual dispatch via
// convert_cost_to_z3(): R2EGroundedEncoder evaluates at the chain-substituted
// intermediate state σ^t_{prev(i)}; other encoders at x_t.
//
// Abstract cost: for each abstract action, uses the action's actual cost as
// the lower bound. For constant costs this is exact; for SDAC, externally
// provided cost_lower_bounds_ (computed from NumericRelaxedPlanningGraph
// interval analysis at fixpoint) are used. This is a sound lower bound: the
// RPG fixpoint bounds over-approximate all reachable fluent values, so
// min(cost_expr) over those bounds is ≤ the true minimum cost in any
// reachable state.
// ============================================================================

z3::expr AbstractSuffixEncoder::build_concrete_cost_term(int i) {
    z3::expr cost = ctx_.real_val(0);
    auto& vf = encoder_.get_variable_factory();

    for (const auto& action : problem_.actions()) {
        const z3::expr& a_var = vf.get_action_variable(action, i);
        z3::expr c_a = encoder_.convert_cost_to_z3(action, i);
        cost = cost + z3::ite(a_var, c_a, ctx_.real_val(0));
    }
    return cost;
}

z3::expr AbstractSuffixEncoder::build_abstract_cost_term() {
    z3::expr cost = ctx_.real_val(0);
    const auto& actions = problem_.actions();
    for (size_t i = 0; i < action_abs_.size(); ++i) {
        // For constant costs: exact via Z3 expression.
        // For SDAC: use externally provided lower bound from RPG interval analysis.
        z3::expr c_a = (!cost_lower_bounds_.empty())
            ? ctx_.real_val(std::to_string(cost_lower_bounds_[i]).c_str())
            : encoder_.convert_expr_id_to_z3(actions[i].cost_id(), 0);
        cost = cost + z3::ite(action_abs_[i], c_a, ctx_.real_val(0));
    }
    return cost;
}

} // namespace rantanplan
