#include "static_fluent_pass.hpp"
#include "../util/ipar_names.hpp"
#include "../util/logger.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/stats.hpp"
#include <cmath>
#include <stdexcept>
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
// Constant folding for ARRAY_READ on ARRAY_CONSTANT with constant index
// ============================================================================

// Evaluates ARRAY_READ(ARRAY_CONSTANT(e0, ..., eN), const_idx) -> e_idx.
// This enables the static-fluent pass to propagate constant array values through
// goals and preconditions that use array-read expressions (e.g. a static card layout
// accessed as card_at[1][1]).  Nested N-D reads are handled by recursion in simplify_expr.
static ExprID try_fold_array_read(ExprPool& pool, const std::vector<ExprID>& children) {
    // children[0] = head (ARRAY_READ op symbol), children[1] = array, children[2] = index
    if (children.size() != 3) return EXPR_NULL;

    ExprID arr_id = children[1];
    ExprID idx_id = children[2];

    auto idx_val = try_get_numeric(pool, idx_id);
    if (!idx_val) return EXPR_NULL;
    auto idx = static_cast<int64_t>(*idx_val);
    if (idx < 0) return EXPR_NULL;

    if (!pool.is_function_application(arr_id)) return EXPR_NULL;
    if (pool.op(arr_id) != ExprOperator::ARRAY_CONSTANT) return EXPR_NULL;

    size_t n = pool.argument_count(arr_id);
    if (static_cast<size_t>(idx) >= n) return EXPR_NULL;

    ExprID elem = pool.argument(arr_id, static_cast<size_t>(idx));

    return elem;
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
// Normalize a fully-grounded FUNCTION_APPLICATION fluent to its STATE_VARIABLE
// ============================================================================

// After ARRAY_READ folding, an expression like connections(card_0, S_dir) is
// rebuilt as FUNCTION_APPLICATION because that was the original kind.  The RPG
// only looks up boolean reachability for STATE_VARIABLE nodes, not for
// FUNCTION_APPLICATION nodes, so this FUNCTION_APPLICATION would evaluate to
// UNKNOWN and the action would be incorrectly pruned.
//
// This helper interns the STATE_VARIABLE version of the same expression and
// returns it if it already exists in the pool AND is a known grounded fluent.
// If the STATE_VARIABLE doesn't exist in the pool (wasn't created during
// protobuf parsing) or isn't a grounded fluent, returns EXPR_NULL.
static ExprID try_normalize_to_state_variable(
        ExprPool& pool, ExprID fa_id,
        const std::unordered_set<ExprID>& grounded_svs) {
    if (!pool.is_function_application(fa_id)) return EXPR_NULL;

    // Head may be either a FLUENT_SYMBOL (kind=3) or a FUNCTION_SYMBOL (kind=4).
    // In UP's protobuf, a user-defined fluent applied in a precondition is encoded
    // as FUNCTION_APPLICATION with a FUNCTION_SYMBOL head (kind=4, op=UNKNOWN).
    // The same fluent applied in the initial state is STATE_VARIABLE with a
    // FLUENT_SYMBOL head (kind=3, op=-1).  We must derive the FLUENT_SYMBOL from
    // the FUNCTION_SYMBOL so the rebuilt SV node matches the one already in the pool.
    ExprID fa_head = pool.head_symbol_id(fa_id);
    bool head_is_function = pool.is_function_symbol(fa_head);
    bool head_is_fluent   = pool.is_fluent_symbol(fa_head);
    if (!head_is_function && !head_is_fluent) return EXPR_NULL;

    // All arguments must be object constants (string payloads = object names).
    size_t nargs = pool.argument_count(fa_id);
    for (size_t i = 0; i < nargs; ++i) {
        ExprID arg = pool.argument(fa_id, i);
        if (!pool.is_constant(arg) || !pool.payload_is_string(arg)) return EXPR_NULL;
    }

    // Derive the FLUENT_SYMBOL head that STATE_VARIABLE nodes use.
    // FUNCTION_SYMBOL has kind=4, op=UNKNOWN; FLUENT_SYMBOL has kind=3, op=-1.
    // Both carry the same string payload (fluent name) and type_id.
    ExprID fluent_head_id;
    if (head_is_fluent) {
        fluent_head_id = fa_head;
    } else {
        const ExprNode& fn_node = pool.get(fa_head);
        ExprNode fluent_sym;
        fluent_sym.kind    = static_cast<int>(ExprKind::FLUENT_SYMBOL);
        fluent_sym.op      = -1;
        fluent_sym.type_id = fn_node.type_id;
        fluent_sym.payload = fn_node.payload;  // fluent name string
        fluent_sym.children.clear();
        fluent_head_id = pool.intern(std::move(fluent_sym));
    }

    // Build STATE_VARIABLE node: [fluent_head, arg0, arg1, ...]
    // STATE_VARIABLE nodes have op=-1 (never set by the parser) and empty payload.
    const ExprNode& orig = pool.get(fa_id);
    ExprNode sv_node;
    sv_node.kind    = static_cast<int>(ExprKind::STATE_VARIABLE);
    sv_node.op      = -1;
    sv_node.type_id = orig.type_id;
    sv_node.children.clear();
    sv_node.children.push_back(fluent_head_id);
    for (size_t i = 0; i < nargs; ++i) {
        sv_node.children.push_back(pool.argument(fa_id, i));
    }
    // STATE_VARIABLE nodes carry no payload.
    ExprID sv_id = pool.intern(std::move(sv_node));

    if (grounded_svs.count(sv_id)) return sv_id;
    return EXPR_NULL;
}

// ============================================================================
// Combined substitute + constant fold walker
// ============================================================================

static ExprID simplify_expr(ExprPool& pool, ExprID expr,
                             const std::unordered_map<ExprID, ExprID>& static_values,
                             const std::unordered_set<ExprID>& grounded_svs) {
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
        ExprID nc = simplify_expr(pool, child, static_values, grounded_svs);
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
        } else if (op == ExprOperator::ARRAY_READ) {
            ExprID folded = try_fold_array_read(pool, new_children);
            if (folded.valid()) return folded;
        }
    }

    // Intern the new node with substituted children.
    // If the result is a FUNCTION_APPLICATION fluent with all-constant object
    // arguments (happens when ARRAY_READ folding resolved args like card_at[0][0]
    // → card_0), try to normalize it back to the grounded STATE_VARIABLE.
    // The RPG only recognizes STATE_VARIABLE nodes for boolean reachability, so
    // leaving it as FUNCTION_APPLICATION would make the precondition evaluate to
    // UNKNOWN and incorrectly prune the action.
    if (static_cast<ExprKind>(orig_kind) == ExprKind::FUNCTION_APPLICATION) {
        // Build the candidate node first, then check for SV normalization.
        ExprNode candidate;
        candidate.kind    = orig_kind;
        candidate.op      = orig_op;
        candidate.type_id = orig_type_id;
        candidate.children = new_children;
        candidate.payload  = orig_payload;
        ExprID candidate_id = pool.intern(candidate);
        ExprID sv_id = try_normalize_to_state_variable(pool, candidate_id, grounded_svs);
        if (sv_id.valid()) return sv_id;
        return candidate_id;
    }

    ExprNode new_node;
    new_node.kind = orig_kind;
    new_node.op = orig_op;
    new_node.type_id = orig_type_id;
    new_node.children = std::move(new_children);
    new_node.payload = std::move(orig_payload);
    return pool.intern(std::move(new_node));
}

// Returns true if `eid` or any of its descendants is an ARRAY_READ node.
static bool contains_array_read(ExprID eid, const ExprPool& pool) {
    if (!eid.valid()) return false;
    if (!pool.is_function_application(eid)) return false;
    if (pool.op(eid) == ExprOperator::ARRAY_READ) return true;
    for (size_t i = 0; i < pool.argument_count(eid); ++i)
        if (contains_array_read(pool.argument(eid, i), pool)) return true;
    return false;
}

// Throws if any grounded fluent takes an array read as one of its arguments.
// Reads elsewhere are fine — as an effect value (assign(robot_at, board[r][c])),
// as a write target, or under a set/arithmetic operator ((member N board[r][c]))
// — because those consume the read's value directly instead of needing the
// enclosing fluent to be grounded once per possible value.
static void reject_array_reads_in_fluent_args(const Problem& problem) {
    const ExprPool& pool = problem.pool();
    for (ExprID eid : problem.grounded_fluents()) {
        if (!pool.is_state_variable(eid)) continue;
        for (ExprID arg : pool.arguments(eid)) {
            if (!contains_array_read(arg, pool)) continue;
            throw std::runtime_error(
                "Array reads are not supported as fluent arguments: " +
                pool.to_string(eid) + ". Rewrite the domain so the read supplies "
                "a value rather than a fluent argument — store the property in "
                "the array cell itself (e.g. an (array N (set direction)) queried "
                "with (member ?d (read (grid) ?r ?c))) instead of indirecting "
                "through an object identity read out of the array.");
        }
    }
}

// ============================================================================
// Pass implementation
// ============================================================================

void StaticFluentPass::apply(PipelineResult& result) const {
    ScopedTimer timer("static_fluent.time_ms");
    const auto& problem = result.problem;
    ExprPool& pool = *problem.pool_ptr();

    // Step 1: Identify fluents modified by any effect.
    // For scalar fluents the effect fluent_id is just SV(f) — straightforward.
    // For array writes the expression tree is nested, so we peel it:
    //
    //   1D write:  ARRAY_WRITE(SV(board), i)
    //     → mark ARRAY_WRITE(...) and SV(board)
    //
    //   2D write:  ARRAY_WRITE(ARRAY_READ(SV(board), i), j)
    //     → mark ARRAY_WRITE(...), then peel ARRAY_WRITE → ARRAY_READ(SV(board), i),
    //       then peel ARRAY_READ → SV(board)
    //
    // Both peeling loops (ARRAY_WRITE then ARRAY_READ) run until they hit a node
    // that is neither, so 3D+ writes are handled automatically.
    std::unordered_set<ExprID> modified_fluents;
    for (const auto& action : problem.actions()) {
        for (const auto& effect : action.effects()) {
            ExprID fid = effect.effect_expression().fluent_id();
            modified_fluents.insert(fid);
            while (pool.is_function_application(fid) &&
                   pool.op(fid) == ExprOperator::ARRAY_WRITE &&
                   pool.argument_count(fid) >= 1) {
                fid = pool.argument(fid, 0);
                modified_fluents.insert(fid);
            }
            while (pool.is_function_application(fid) &&
                   pool.op(fid) == ExprOperator::ARRAY_READ &&
                   pool.argument_count(fid) >= 1) {
                fid = pool.argument(fid, 0);
                modified_fluents.insert(fid);
            }
        }
    }

    // [XTS] IPAR cell-write propagation: bidirectional sync between the array SV
    // and its cell SVs ("board[0]", "board[1]", ...) that IPAR created.
    //
    // Forward (array write → cell SVs):
    //   An action writes ARRAY_WRITE(SV(board), 2) — this modifies board[2].
    //   Without the forward pass, board[2] looks unmodified (no effect has fluent_id
    //   == SV(board[2]) directly).  StaticFluentPass would substitute board[2]'s
    //   initial value into any goal mentioning it — turning e.g. (= board[2] 5)
    //   into (= 0 5) = FALSE, making the problem appear unsolvable even when it isn't.
    //
    // Reverse (cell SV modified → parent array):
    //   An IPAR-generated effect writes cell SV board[2] directly.  Without the
    //   reverse pass, SV(board) itself looks unmodified and gets classified as
    //   static — its initial ARRAY_CONSTANT would be substituted everywhere,
    //   hiding the fact that board is being updated by the actions.
    {
        // Build base-name → parent SV map and cell-name → SV map from grounded fluents.
        std::unordered_map<std::string, ExprID> base_to_parent_sv;
        std::unordered_map<std::string, ExprID> cell_name_to_sv;
        for (ExprID gf : problem.grounded_fluents()) {
            if (!pool.is_state_variable(gf)) continue;
            if (pool.argument_count(gf) != 0) continue;
            ExprID head = pool.head_symbol_id(gf);
            if (!pool.payload_is_string(head)) continue;
            const std::string& name = pool.payload_string(head);
            if (name.find('[') == std::string::npos) {
                base_to_parent_sv.emplace(name, gf);
            } else {
                cell_name_to_sv.emplace(name, gf);
            }
        }

        std::vector<ExprID> to_add;

        // Forward pass: ARRAY_WRITE(base_sv, k_const) → mark IPAR cell SV "base[k]".
        for (ExprID fid : modified_fluents) {
            if (!pool.is_function_application(fid)) continue;
            if (pool.op(fid) != ExprOperator::ARRAY_WRITE) continue;
            if (pool.argument_count(fid) < 2) continue;
            ExprID base_arg = pool.argument(fid, 0);
            ExprID idx_arg  = pool.argument(fid, 1);
            if (!pool.is_constant(idx_arg) || !pool.payload_is_int(idx_arg)) continue;
            if (!pool.is_state_variable(base_arg)) continue;
            ExprID bh = pool.head_symbol_id(base_arg);
            if (!pool.payload_is_string(bh)) continue;
            int64_t k = pool.payload_int(idx_arg);
            std::string cell_name = pool.payload_string(bh) + "[" + std::to_string(k) + "]";
            auto it = cell_name_to_sv.find(cell_name);
            if (it != cell_name_to_sv.end()) to_add.push_back(it->second);
        }
        for (ExprID sv : to_add) modified_fluents.insert(sv);

        // Reverse pass: IPAR cell SV "base[k]" modified → mark parent SV "base".
        to_add.clear();
        for (ExprID fid : modified_fluents) {
            if (!pool.is_state_variable(fid)) continue;
            if (pool.argument_count(fid) != 0) continue;
            ExprID head = pool.head_symbol_id(fid);
            if (!pool.payload_is_string(head)) continue;
            const std::string& name = pool.payload_string(head);
            size_t bracket = name.find('[');
            if (bracket == std::string::npos || bracket == 0) continue;
            std::string base = name.substr(0, bracket);
            auto it = base_to_parent_sv.find(base);
            if (it != base_to_parent_sv.end()) to_add.push_back(it->second);
        }
        for (ExprID sv : to_add) modified_fluents.insert(sv);
    }

    // Step 2: Static fluents = grounded fluents NOT in modified set AND that don't
    // indirectly depend on a modified array through an ARRAY_READ argument.
    // Example: connections(read(card_at, r, c), dir)
    //   connections itself is never written to — but its effective value depends on
    //   card_at, which IS modified by rotate actions.  CWA assigned it FALSE (no
    //   explicit init), but that value is wrong once card_at changes.  Treating it
    //   as static would substitute FALSE into every precondition that mentions it,
    //   incorrectly pruning actions that should be reachable.
    std::function<bool(ExprID)> references_modified_array = [&](ExprID eid) -> bool {
        if (!eid.valid()) return false;
        if (!pool.is_function_application(eid)) return false;
        if (pool.op(eid) == ExprOperator::ARRAY_READ) {
            // The base of the ARRAY_READ chain — if it's a modified state variable, return true.
            ExprID cur = eid;
            while (pool.is_function_application(cur) &&
                   pool.op(cur) == ExprOperator::ARRAY_READ &&
                   pool.argument_count(cur) >= 1) {
                cur = pool.argument(cur, 0);
            }
            if (modified_fluents.count(cur)) return true;
        }
        for (size_t i = 0; i < pool.argument_count(eid); ++i)
            if (references_modified_array(pool.argument(eid, i))) return true;
        return false;
    };

    std::unordered_set<ExprID> static_fluent_set;
    for (ExprID gf : problem.grounded_fluents()) {
        if (modified_fluents.count(gf)) continue;
        // Also exclude fluents whose arguments transitively reference a modified
        // array.  Array reads as fluent arguments are unsupported and rejected at
        // the end of this pass — but only if they survive it.  Treating one as
        // static would substitute in its CWA default (false) and delete it from
        // the grounded fluents, turning a clear error into a silent UNSOLVABLE.
        if (!pool.is_state_variable(gf)) { static_fluent_set.insert(gf); continue; }
        bool depends_on_modified = false;
        for (ExprID arg : pool.arguments(gf)) {
            if (references_modified_array(arg)) { depends_on_modified = true; break; }
        }
        if (!depends_on_modified) static_fluent_set.insert(gf);
    }

    if (static_fluent_set.empty()) {
        Logger::instance().component(VerbosityLevel::INFO, name(), {
            {"status", "no static fluents found"}
        });
        return;
    }

    // Step 3: Build value map from initial state.
    std::unordered_map<ExprID, ExprID> static_values;
    for (const auto& assignment : problem.initial_state()) {
        if (static_fluent_set.count(assignment.fluent_id())) {
            static_values[assignment.fluent_id()] = assignment.value_id();
        }
    }

    // [XTS] Step 3a: Replace CWA sentinel values for IPAR-generated cell fluents
    // with the actual element values from the parent array.
    //
    // IPAR creates arity-0 scalar fluents for every concrete cell it accesses:
    //   card_at[0][0], card_at[0][1], card_at[1][0], ...
    // Only card_at as a whole has an explicit initial assignment (an ARRAY_CONSTANT).
    // The cell SVs got the CWA sentinel -1 (object type, no real value assigned).
    //
    // If we substitute -1 into preconditions, e.g.:
    //   (= card_at[0][0] card_A)  →  (= -1 card_A)  →  FALSE
    // ... actions that should be enabled at the first step get pruned away.
    //
    // Fix: for each cell SV with sentinel -1, index into the parent ARRAY_CONSTANT:
    //   card_at = ARRAY_CONSTANT(card_A, card_B, card_C, ...)
    //   card_at[0] → card_A   (replaces -1 with the correct object index)

    // Build base-name → (head_sym_id, array_constant_id) map.
    std::unordered_map<std::string, ExprID> arr_basename_to_val;
    for (const auto& [sid, vid] : static_values) {
        if (!pool.is_state_variable(sid)) continue;
        if (pool.argument_count(sid) != 0) continue;   // arity-0 only
        if (!pool.is_function_application(vid)) continue;
        if (pool.op(vid) != ExprOperator::ARRAY_CONSTANT) continue;
        // The head symbol carries the fluent name string.
        ExprID head = pool.head_symbol_id(sid);
        if (!pool.payload_is_string(head)) continue;
        arr_basename_to_val[pool.payload_string(head)] = vid;
    }

    // Helper: walk nested ARRAY_CONSTANTs with an integer index list.
    auto index_into_array = [&](ExprID arr_val, const std::vector<int64_t>& idxs) -> ExprID {
        ExprID cur = arr_val;
        for (int64_t idx : idxs) {
            if (!pool.is_function_application(cur)) return EXPR_NULL;
            if (pool.op(cur) != ExprOperator::ARRAY_CONSTANT) return EXPR_NULL;
            size_t n = pool.argument_count(cur);
            if (idx < 0 || static_cast<size_t>(idx) >= n) return EXPR_NULL;
            cur = pool.argument(cur, static_cast<size_t>(idx));
        }
        return cur;
    };

    if (!arr_basename_to_val.empty()) {
        for (auto& [sv_id, val_id] : static_values) {
            if (!pool.is_state_variable(sv_id)) continue;
            if (pool.argument_count(sv_id) != 0) continue;  // arity-0 cell SVs only

            // Value must be the CWA object sentinel int -1.
            if (!pool.is_constant(val_id) || !pool.payload_is_int(val_id)) continue;
            if (pool.payload_int(val_id) != -1) continue;

            // Name must follow "base[i][j]..." notation.
            ExprID head = pool.head_symbol_id(sv_id);
            if (!pool.payload_is_string(head)) continue;
            const std::string& name = pool.payload_string(head);

            auto parsed = parse_ipar_cell_name(name);
            if (!parsed) continue;
            auto& [base, idxs] = *parsed;

            auto arr_it = arr_basename_to_val.find(base);
            if (arr_it == arr_basename_to_val.end()) continue;

            ExprID actual_val = index_into_array(arr_it->second, idxs);
            if (actual_val.valid()) {
                val_id = actual_val;  // replace CWA sentinel with real cell value
            }
        }
    }

    // Step 3b: Build the set of all grounded STATE_VARIABLEs.
    // Used by simplify_expr to normalize FUNCTION_APPLICATION fluents that
    // become fully grounded after ARRAY_READ folding back to STATE_VARIABLEs.
    std::unordered_set<ExprID> grounded_svs;
    for (ExprID gf : problem.grounded_fluents()) {
        if (pool.is_state_variable(gf)) grounded_svs.insert(gf);
    }

    // Step 4: Simplify all action expressions
    std::vector<Action> new_actions;
    new_actions.reserve(problem.actions().size());

    for (const auto& action : problem.actions()) {
        Action new_action = action;

        // Simplify precondition
        if (new_action.has_precondition()) {
            ExprID new_pre = simplify_expr(pool, action.precondition_id(), static_values, grounded_svs);
            if (new_pre != action.precondition_id()) {
                new_action.set_precondition_id(new_pre);
            }
        }

        // Simplify effects
        for (auto& effect : new_action.mutable_effects()) {
            const auto& ee = effect.effect_expression();
            ExprID new_value = simplify_expr(pool, ee.value_id(), static_values, grounded_svs);
            ExprID new_cond = ee.is_conditional()
                ? simplify_expr(pool, ee.condition_id(), static_values, grounded_svs)
                : ee.condition_id();

            if (new_value != ee.value_id() || new_cond != ee.condition_id()) {
                EffectExpression new_ee(ee.kind(), ee.fluent_id(), new_value, new_cond, &pool);
                effect.set_effect_expression(new_ee);
            }
        }

        // Simplify cost expression
        if (new_action.has_explicit_cost()) {
            ExprID new_cost = simplify_expr(pool, action.cost_id(), static_values, grounded_svs);
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
        ExprID new_gid = simplify_expr(pool, goal.goal_id(), static_values, grounded_svs);
        if (new_gid != goal.goal_id()) {
            new_goal.set_goal_id(new_gid);
        }
        new_goals.push_back(std::move(new_goal));
    }

    // Step 6: Filter initial state.
    // Problems WITHOUT array/set fluents (standard PDDL): remove static fluent
    // assignments — their values were fully substituted in Steps 4/5 (pre-XTS
    // behavior, keeps encodings unchanged for standard problems).
    // [XTS] Problems WITH array/set fluents: keep static assignments. They stay
    // load-bearing after substitution — expand_object_sv_arg's ITE expansion
    // references simple static fluents (e.g. is-smooth(surface_A)) by NAME at
    // encoding time, not through ExprID
    // substitution, so dropping them makes goals falsely unreachable.  Only
    // IPAR-generated cell SVs (e.g. "card_at[0][0]") are dropped: they have no
    // declared fluent, and keeping them would crash the encoder's fluent lookup.
    bool problem_has_arrays = false;
    for (ExprID gf : problem.grounded_fluents()) {
        const Type* vt = problem.type_for_id(gf);
        if (vt && (vt->is_array() || vt->is_set())) { problem_has_arrays = true; break; }
    }

    std::vector<Assignment> new_initial_state;
    new_initial_state.reserve(problem.initial_state().size());
    for (const auto& assignment : problem.initial_state()) {
        ExprID fid = assignment.fluent_id();
        if (static_fluent_set.count(fid)) {
            if (!problem_has_arrays) {
                continue;  // static fluent — substituted away (pre-XTS behavior)
            }
            if (pool.is_state_variable(fid) && pool.argument_count(fid) == 0) {
                ExprID head = pool.head_symbol_id(fid);
                if (pool.payload_is_string(head) &&
                    pool.payload_string(head).find('[') != std::string::npos) {
                    continue;  // [XTS] IPAR cell SV — fully substituted away
                }
            }
        }
        new_initial_state.push_back(assignment);
    }

    // Step 7: Build new problem (re-collects grounded fluents automatically)
    size_t old_fluent_count = problem.grounded_fluent_count();
    result.problem = problem.with_simplified(
        std::move(new_actions), std::move(new_goals),
        std::move(new_initial_state));
    size_t new_fluent_count = result.problem.grounded_fluent_count();

    // Step 8: Reject array reads left inside fluent arguments.
    // A read whose base array is static was folded to a plain object constant in
    // Steps 4/5 (try_fold_array_read), so anything surviving here reads an array
    // the actions write to — e.g. connections(card_at[r][c], N).  Encoding that
    // needs a value-dependent ITE over every object, which the encoder, the frame
    // axioms and the RPG no longer support.  Fail here, before action pruning can
    // draw conclusions from fluents it cannot evaluate.
    reject_array_reads_in_fluent_args(result.problem);

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
