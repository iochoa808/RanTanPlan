#include "cwa_initial_state_pass.hpp"
#include "../util/ipar_names.hpp"
#include "../util/logger.hpp"
#include "../util/scoped_timer.hpp"
#include <algorithm>
#include <unordered_set>

namespace rantanplan {

void CWAInitialStatePass::apply(PipelineResult& result) const {
    ScopedTimer timer("cwa.time_ms");

    auto& problem = result.problem;
    auto pool = problem.pool_ptr();

    // Build set of fluent ExprIDs that already have an initial assignment.
    std::unordered_set<ExprID> assigned;
    for (const auto& assignment : problem.initial_state()) {
        assigned.insert(assignment.fluent_id());
    }

    // [XTS] Build a lookup from array fluent name -> initial ARRAY_CONSTANT ExprID.
    // Used below to seed IPAR cell SVs ("board[0]", "board[1]", ...) with real element
    // values instead of the generic default 0.
    // Example:
    // board = ARRAY_CONSTANT(5, 3, 7) -> parent_array_init_value["board"] = ARRAY_CONSTANT(5, 3, 7)
    // Later, board[1] can be seeded as 3 instead of 0.
    std::unordered_map<std::string, ExprID> parent_array_init_value;
    for (const auto& assignment : problem.initial_state()) {
        // Only arity-0 array-typed SVs are parents of IPAR cells.
        ExprID fid = assignment.fluent_id();
        if (!pool->is_state_variable(fid)) continue;
        if (pool->argument_count(fid) != 0) continue;
        const Type* at = problem.type_for_id(fid);
        if (!at || !at->is_array()) continue;
        // Only literal ARRAY_CONSTANT values can be indexed into element-wise.
        ExprID val = assignment.value_id();
        if (!pool->is_function_application(val)) continue;
        if (pool->op(val) != ExprOperator::ARRAY_CONSTANT) continue;
        // Key by the fluent's name: what cell names embed.
        ExprID h = pool->head_symbol_id(fid);
        if (pool->payload_is_string(h))
            parent_array_init_value[pool->payload_string(h)] = val;
    }

    // [XTS] Names of arity-0 array/set-typed grounded fluents. A "base[i]..."
    // name only counts as an IPAR cell if "base" is here.
    std::unordered_set<std::string> array_parent_names;
    for (ExprID gf : problem.grounded_fluents()) {
        // Same shape test as above (arity-0 SV), only the *name* is needed here.
        if (!pool->is_state_variable(gf)) continue;
        if (pool->argument_count(gf) != 0) continue;
        const Type* gt = problem.type_for_id(gf);
        if (!gt || (!gt->is_array() && !gt->is_set())) continue;
        ExprID gh = pool->head_symbol_id(gf);
        if (pool->payload_is_string(gh))
            array_parent_names.insert(pool->payload_string(gh));
    }

    // Find grounded fluents missing from the initial state.
    std::vector<Assignment> defaults;
    for (ExprID eid : problem.grounded_fluents()) {
        if (assigned.count(eid)) continue;

        // [XTS] IPAR cell SV (e.g. "board[1]"): seed with the real element from the
        // parent array's ARRAY_CONSTANT instead of the generic default 0.
        // Example: board = ARRAY_CONSTANT(5, 3, 7), board[1] unset
        //   Without this fix: board[1] = 0  → RPG sees wrong bound, may wrongly
        //                                      restrict actions that need value 3.
        //   With this fix:    board[1] = 3  → RPG and InvariantOracle are correct.
        if (pool->is_state_variable(eid) && pool->argument_count(eid) == 0) {
            ExprID h = pool->head_symbol_id(eid);
            if (pool->payload_is_string(h)) {
                // "board[1][2]" -> base "board" + indices {1, 2}
                auto parsed = parse_ipar_cell_name(pool->payload_string(h));
                if (parsed) {
                    auto& [base, idxs] = *parsed;
                    // Nothing to seed from unless the parent has an initial value.
                    auto it = parent_array_init_value.find(base);
                    if (it != parent_array_init_value.end()) {
                        // Walk into nested ARRAY_CONSTANTs, one level per index.
                        // board[1][2] → ARRAY_CONSTANT[1] → ARRAY_CONSTANT[2] → elem.
                        // Out-of-bounds indices fail the size_t cast and fall through
                        // to the generic default below.
                        ExprID elem_val = it->second;
                        bool ok = true;
                        for (int64_t k : idxs) {
                            if (!pool->is_function_application(elem_val) ||
                                pool->op(elem_val) != ExprOperator::ARRAY_CONSTANT ||
                                static_cast<size_t>(k) >= pool->argument_count(elem_val)) {
                                ok = false;
                                break;
                            }
                            elem_val = pool->argument(elem_val, static_cast<size_t>(k));
                        }
                        if (ok) {
                            defaults.emplace_back(eid, elem_val, &problem.pool());
                            continue;
                        }
                    }
                }
            }
        }

        // Determine the fluent's type and create the appropriate default constant.
        const ExprNode& node = pool->get(eid);
        int fluent_type_id = node.type_id;
        if (fluent_type_id < 0) continue;  // unknown type — skip

        const Type* type = problem.type_for_id(eid);
        if (!type) continue;

        ExprNode constant_node;
        constant_node.kind = static_cast<int>(ExprKind::CONSTANT);
        constant_node.type_id = fluent_type_id;

        if (type->is_bool()) {
            constant_node.payload = false;
        } else if (type->is_int()) {
            // [XTS] Clamp the default into the declared bounded-int range
            int64_t default_val = 0;
            if (const Type* bi = type->bounded_int_ancestor()) {
                default_val = std::clamp<int64_t>(0, bi->lower_bound(), bi->upper_bound());
            }
            constant_node.payload = default_val;
        } else if (type->is_real()) {
            constant_node.payload = 0.0;
        } else if (type->is_object()) {
            // [XTS] IPAR cell SVs ("base[i]...") get the sentinel -1: objects are
            // encoded as indices in [0, N-1], so -1 is always out of range, and the
            // static-fluent pass later replaces it with the real element
            // from the parent ARRAY_CONSTANT.
            bool is_ipar_cell = false;
            if (pool->is_state_variable(eid) && pool->argument_count(eid) == 0) {
                ExprID h = pool->head_symbol_id(eid);
                if (pool->payload_is_string(h)) {
                    auto parsed = parse_ipar_cell_name(pool->payload_string(h));
                    // Require the parent array/set fluent to actually exist
                    is_ipar_cell = parsed && array_parent_names.count(parsed->first);
                }
            }
            if (!is_ipar_cell) continue;
            constant_node.payload = static_cast<int64_t>(-1);
        } else {
            continue;  // other types — no sensible default
        }

        ExprID value_id = pool->intern(std::move(constant_node));
        defaults.emplace_back(eid, value_id, &problem.pool());
    }

    if (defaults.empty()) {
        Logger::instance().component(VerbosityLevel::VERBOSE, "CWA", {
            {"time", std::to_string(static_cast<int>(timer.elapsed_ms())) + "ms"},
            {"status", "all grounded fluents already assigned"}
        });
        return;
    }

    Logger::instance().component(VerbosityLevel::INFO, "CWA", {
        {"time", std::to_string(static_cast<int>(timer.elapsed_ms())) + "ms"},
        {"added", std::to_string(defaults.size()) + " default assignments"}
    });

    result.problem = problem.with_additional_initial_state(defaults);
}

} // namespace rantanplan
