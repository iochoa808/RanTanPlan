#pragma once

#include "propagator_strategy.hpp"
#include "../../problem/problem.hpp"
#include "../../problem/fluent.hpp"
#include "../../problem/action.hpp"
#include "../../analysis/graph.hpp"
#include <z3++.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rantanplan {

/**
 * @brief Exists propagator extended with lazy frame axiom enforcement.
 *
 * Combines exists-step cycle detection with frame axiom propagation.
 *
 * Design: register fluent state variables with add(). Z3 calls on_fixed
 * when they're assigned. Action variables are already registered by the
 * exists cycle detection. In conflict(fixed), Z3 handles polarity based
 * on current assignment — no need to register negated variables.
 */
class FrameExistsPropagator : public PropagatorStrategy {
public:
    FrameExistsPropagator(z3::solver& solver, const Problem& problem, BaseEncoder& encoder);
    ~FrameExistsPropagator() override = default;

    void on_push() override;
    void on_pop(unsigned num_scopes) override;
    void on_fixed(z3::expr const& ast, z3::expr const& value) override;
    void on_final() override;

    void register_timestep_variables(int timestep) override;
    void cleanup() override;
    std::string get_name() const override { return use_memo_ ? "MemoFramePropagator" : "FrameExistsPropagator"; }
    bool manages_parallelism_constraints() const override { return true; }

    void set_use_memo(bool enabled);

private:
    // ========================================================================
    // Exists-step cycle detection (from ExistsPropagator)
    // ========================================================================
    const Problem* problem_;
    const Z3VariableFactory* variable_factory_;
    const ParallelismStrategy* parallelism_strategy_;
    const InterferenceAnalysis* interference_analyzer_;

    std::vector<std::pair<int, int>> trail_;
    std::vector<size_t> decision_levels_;
    std::unordered_map<int, std::unordered_set<int>> active_actions_per_timestep_;
    std::unordered_map<int, std::vector<std::shared_ptr<z3::expr>>> registered_action_vars_;
    int cycle_count_ = 0;

    void perform_exists_propagation(const Action& action, int timestep, const z3::expr& action_var);
    bool find_cycle_in_active_actions(const std::unordered_set<int>& active_node_ids,
                                      std::vector<int>& cycle);

    // Footprint-indexed potential interference neighbors: action_id → set of action_ids
    // that share at least one fluent in their read/write footprints.
    // Built once at first registration; used to narrow the DFS in cycle detection.
    std::unordered_map<int, std::vector<int>> potential_interferers_;
    bool footprint_index_built_ = false;
    void build_footprint_index();

    // ========================================================================
    // Frame axiom state
    // ========================================================================

    z3::solver* solver_;
    BaseEncoder* encoder_nc_;

    struct EPCEntry {
        const Action* action;
        bool is_conditional;
        int8_t action_state = -1;
        int8_t cond_state = -1;

        bool can_explain() const {
            if (action_state != 1) return false;
            if (!is_conditional) return true;
            return cond_state == 1;
        }
        bool cant_explain() const {
            if (action_state == 0) return true;
            if (is_conditional && cond_state == 0) return true;
            return false;
        }
    };

    struct FrameClause {
        int timestep;
        int8_t eq_state = -1;     // -1=unset, 0=changed, 1=unchanged
        bool is_boolean;

        // Boolean fluents: track individual endpoint values
        int8_t ft_val = -1;
        int8_t ft1_val = -1;

        std::vector<EPCEntry> entries;
        int num_cant_explain = 0;
        int num_can_explain = 0;

        // Ownership flag: true when at least one action with can_explain() is
        // true. When owned, preservation is unnecessary — the fluent change is
        // already explained. Avoids redundant propagate() calls.
        bool owned = false;

        // Memo flag (v5): persistent across backtracks (NOT in trail).
        // When set and persist_clauses is on, skip re-propagation of preservation.
        bool preservation_ever_fired = false;

        FrameClause(int t, bool is_bool)
            : timestep(t), is_boolean(is_bool) {}
    };

    struct VarRole {
        enum Kind { FLUENT_T, FLUENT_T1, ACTION, CONDITION, EQ_BOOL };
        Kind kind;
        size_t clause_idx;
        size_t entry_idx;
    };

    struct FrameTrailEntry {
        size_t clause_idx;
        VarRole::Kind kind;
        size_t entry_idx;
        int8_t prev_state;
        int8_t prev_eq_state;
        int prev_cant_explain;
        int prev_can_explain;
        bool prev_owned;
    };

    std::vector<FrameClause> frame_clauses_;
    std::unordered_map<unsigned, std::vector<VarRole>> frame_var_to_roles_;

    // Flat vectors indexed by clause_idx for O(1) access in conflict/propagation.
    // frame_fluent_ft_[i] / frame_fluent_ft1_[i] = f^t / f^(t+1) Z3 exprs for clause i.
    std::vector<z3::expr> frame_fluent_ft_;
    std::vector<z3::expr> frame_fluent_ft1_;
    // Numeric-only: reified eq_bool for clause i (empty expr for boolean clauses).
    std::vector<z3::expr> frame_eq_bool_;

    // Per-entry vectors indexed by [clause_idx][entry_idx].
    std::vector<std::vector<z3::expr>> frame_action_expr_;
    std::vector<std::vector<z3::expr>> frame_cond_expr_;

    std::unordered_set<unsigned> all_registered_ids_;

    std::vector<FrameTrailEntry> frame_trail_;
    std::vector<size_t> frame_decision_levels_;

    int frame_conflict_count_ = 0;
    int frame_propagation_count_ = 0;
    int frame_on_fixed_count_ = 0;
    int frame_final_violation_count_ = 0;

    // v5 memo: skip redundant re-propagation of preservation
    bool use_memo_ = false;
    bool persist_clauses_ = false;
    int memo_hits_ = 0;
    int first_time_preservations_ = 0;

    void register_frame_variables(int t);
    void check_frame_clause(FrameClause& clause, size_t clause_idx);
    void report_frame_conflict(const FrameClause& clause, size_t clause_idx);
    void propagate_fluent_preservation(const FrameClause& clause, size_t clause_idx);
    void propagate_last_entry(const FrameClause& clause, size_t clause_idx);

    void build_frame_fixed(z3::expr_vector& fixed,
                           const FrameClause& clause, size_t clause_idx,
                           bool include_change, int skip_idx = -1);
};

} // namespace rantanplan
