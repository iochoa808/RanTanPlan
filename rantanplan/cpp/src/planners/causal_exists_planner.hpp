#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "../encoders/base_encoder.hpp"
#include "../abstraction/achievers_analysis.hpp"
#include "base_planner.hpp"
#include <z3++.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rantanplan {

/**
 * @brief Causal Exists planner — core-guided lazy activation over exists-step encoding
 *
 * Adapts CausalLazyR2EPlanner's core-guided lazy activation to the classic
 * exists-step encoding.  Uses per-(action, timestep) blocking literals:
 *
 *     block_a_t → ¬act_a_t
 *
 * UNSAT cores reveal which (action, timestep) pairs are needed.  AchieversAnalysis
 * filters non-goal-relevant actions from cores.  The ExistsPropagator handles
 * interference/cycle detection among activated actions.
 *
 * Replenishment invariant: every action always has at least one blocked copy
 * across all timesteps.  When violated, extend the horizon (add new timestep
 * with all actions blocked).
 *
 * Activity-guided activation: on horizon extension, actions with high VSIDS
 * activity scores are predictively activated at the new timestep.
 */
class CausalExistsPlanner : public BasePlanner {
public:
    CausalExistsPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx);

    Plan search() override;

private:
    // ---- Timestep + blocking management ----

    int current_horizon_ = -1;
    int goal_timestep_ = -1;

    /// A blocking entry: the z3 literal, the action it blocks, and when.
    struct BlockEntry {
        z3::expr lit;
        const Action* action;
        int timestep;
        bool active;  ///< true = still blocking (included in assumptions)
    };

    /// All blocking entries (append-only; entries are deactivated, not removed).
    std::vector<BlockEntry> block_entries_;

    /// Maps block_lit Z3 id → index in block_entries_ (only for active entries).
    std::unordered_map<unsigned, size_t> block_id_to_index_;

    /// Per-action: count of timesteps where this action is still blocked.
    std::unordered_map<const Action*, int> blocked_count_;

    /// Goal assumption literal (refreshed when horizon extends).
    z3::expr goal_assumption_;
    int next_goal_version_ = 0;

    // ---- Achiever data (same as CausalLazyR2EPlanner) ----

    std::unique_ptr<AchieversAnalysis> achievers_;
    std::unordered_map<int, const Action*> action_id_to_ptr_;
    std::unordered_map<ExprID, std::vector<const Action*>> condition_achievers_;
    std::unordered_set<ExprID> init_satisfied_conditions_;
    std::vector<ExprID> goal_condition_ids_;
    std::unordered_map<const Action*, std::vector<ExprID>> action_precondition_ids_;
    std::unordered_set<const Action*> goal_achiever_actions_;
    std::unordered_set<const Action*> goal_relevant_actions_;

    // ---- Relaxed plan (h^ff-style backward extraction) ----

    std::vector<const Action*> relaxed_plan_;

    // ---- Activity tracking (adapted VSIDS) ----

    std::unordered_map<const Action*, double> activity_;
    static constexpr double activity_decay_ = 0.85;
    static constexpr double activation_threshold_ = 0.5;
    int cumulative_core_activations_ = 0;  ///< Total actions activated from cores so far

    // ---- Core-reduce: re-check with core-only assumptions to shrink ----

    static constexpr bool enable_core_reduce_ = false;  ///< Internal switch (off by default)
    static constexpr int max_reduce_passes_ = 3;        ///< Max re-check passes for core shrinking

    // ---- Achiever setup ----

    void initialize_achievers();
    void build_action_id_map();
    void build_condition_achiever_cache();
    void compute_init_satisfied_conditions();
    void extract_relaxed_plan();

    // ---- Timestep management ----

    void add_timestep(int t);
    void refresh_goal(int t);
    void extend_horizon();

    // ---- Search loop helpers ----

    z3::expr_vector build_assumptions();
    z3::expr_vector reduce_core(const z3::expr_vector& core);
    int process_core(const z3::expr_vector& core);
    void cascade_bump(const std::vector<const Action*>& seeds, double bump_amount);
    int predictive_activate(int timestep);
    void decay_activity();
    bool activate_action_at(const Action* action, int timestep);

    // ---- Plan extraction ----

    Plan extract_plan(const z3::model& model);
};

} // namespace rantanplan
