#pragma once

#include "../propagator_module.hpp"
#include "../propagator_strategy.hpp"
#include <z3++.h>
#include <map>

namespace rantanplan {

/**
 * @brief Lazy frame axiom enforcement module.
 *
 * Registers fluent state variables with Z3. When assignments arrive via
 * on_fixed, incrementally maintains per-clause counters tracking which
 * actions can/cannot explain a fluent change. Raises conflicts when no
 * action can explain a change, or propagates preservation when the fluent
 * must remain unchanged.
 *
 * Owns its own trail (frame_trail_ / frame_decision_levels_) for
 * push/pop of frame clause state.
 */
class FrameAxiomModule : public PropagatorModule {
public:
    std::string get_name() const override { return "FrameAxiom"; }

    void on_push() override;
    void on_pop(unsigned num_scopes) override;
    void on_fixed(const z3::expr& ast, const z3::expr& value) override;
    void on_final() override;
    void register_timestep_variables(int timestep) override;
    void cleanup() override;

private:
    // --- Frame clause data structures ---

    struct EPCEntry {
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
        ExprID fluent_id;
        int8_t eq_state = -1;
        bool is_boolean;
        int8_t ft_val = -1;
        int8_t ft1_val = -1;

        std::vector<EPCEntry> entries;
        int num_cant_explain = 0;
        int num_can_explain = 0;

        bool owned() const { return num_can_explain > 0; }

        FrameClause(int t, ExprID fid, bool is_bool)
            : timestep(t), fluent_id(fid), is_boolean(is_bool) {}
    };

    struct VarRole {
        enum Kind { FLUENT_T, FLUENT_T1, ACTION, CONDITION, EQ_BOOL };
        Kind kind;
        size_t clause_idx;
        size_t entry_idx;
    };

    struct FrameTrailEntry {
        uint32_t clause_idx;
        uint32_t entry_idx;
        VarRole::Kind kind;
        int8_t prev_state;
    };

    // --- State ---

    std::vector<FrameClause> frame_clauses_;
    std::unordered_map<unsigned, std::vector<VarRole>> frame_var_to_roles_;

    std::vector<z3::expr> frame_fluent_ft_;
    std::vector<z3::expr> frame_fluent_ft1_;
    std::vector<z3::expr> frame_eq_bool_;

    std::vector<std::vector<z3::expr>> frame_action_expr_;
    std::vector<std::vector<z3::expr>> frame_cond_expr_;

    // Condition reification cache: (condition ExprID, timestep) -> reified z3::expr
    std::map<std::pair<ExprID, int>, z3::expr> cond_reification_cache_;

    // Private trail
    std::vector<FrameTrailEntry> frame_trail_;
    std::vector<size_t> frame_decision_levels_;

    // Stats
    int frame_conflict_count_ = 0;
    int frame_propagation_count_ = 0;
    int frame_on_fixed_count_ = 0;
    int frame_final_violation_count_ = 0;
    int frame_vars_registered_ = 0;

    // --- Helpers ---

    void recompute_derived(FrameClause& clause);
    void update_epc_entry(FrameClause& clause, EPCEntry& entry,
                          int8_t& field, int8_t new_val);
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
