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
 * @brief PDLA planner — Property-Directed Lazy Activation
 *
 * Core-guided lazy activation over exists-step encoding.
 *
 * Change 1 (float activation): one blocking literal per action (not per
 * timestep).  blk_a → ¬act_a_t0 ∧ ¬act_a_t1 ∧ ...  Activating an action
 * makes it available at ALL timesteps; the solver decides placement.
 *
 * See docs/pdla-proposal.md for the full design document.
 */
class PDLAPlanner : public BasePlanner {
public:
    PDLAPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx);
    ~PDLAPlanner() override = default;

    void set_achievers(std::unique_ptr<AchieversAnalysis> achievers);

    Plan search() override;

protected:
    // ---- Timestep + blocking management (float activation) ----

    int current_horizon_ = -1;
    int goal_timestep_ = -1;

    /// Per-action blocking literal: blk_a → ¬act_a_t for all t
    std::unordered_map<const Action*, z3::expr> block_lit_;

    /// Actions currently blocked (blocking lit included in assumptions)
    std::unordered_set<const Action*> blocked_;

    /// Actions that have been activated (constraints encoded at all timesteps)
    std::unordered_set<const Action*> activated_;

    /// Z3 expr id → action pointer (for parsing cores)
    std::unordered_map<unsigned, const Action*> blk_id_to_action_;

    /// Goal assumption literal (refreshed when horizon extends).
    z3::expr goal_assumption_;
    int next_goal_version_ = 0;

    // ---- Achiever data ----

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
    std::unordered_set<const Action*> relaxed_plan_set_;

    // ---- Activity tracking (adapted VSIDS) ----

    std::unordered_map<const Action*, double> activity_;
    static constexpr double activity_decay_ = 0.85;
    static constexpr double activation_threshold_ = 0.5;
    int cumulative_core_activations_ = 0;

    // ---- Lazy population tracking ----

    /// Tracks which timesteps each activated action has been encoded at
    std::unordered_map<const Action*, std::unordered_set<int>> action_encoded_at_;

    // Cached downcast to avoid repeated dynamic_cast in hot path
    class GroundedEncoder* grounded_encoder_ = nullptr;
    GroundedEncoder& grounded_encoder();

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
    int process_core(const z3::expr_vector& core);
    void cascade_bump(const std::vector<const Action*>& seeds, double bump_amount);
    int predictive_activate(int timestep);
    void decay_activity();
    void activate_action(const Action* action);
    void encode_action_at(const Action* action, int timestep);

    // ---- Guided activation ----

    int guided_substitutions_ = 0;
    int guided_fallbacks_ = 0;
    const Action* select_best_achiever_for(const Action* core_action) const;

    // ---- Tracked preconditions ----

    struct TrackedPrecond {
        const Action* action;
        int timestep;
        ExprID condition;
    };
    std::unordered_map<unsigned, TrackedPrecond> tracked_precond_id_;

    // ---- Plan extraction ----

    Plan extract_plan(const z3::model& model);
};

} // namespace rantanplan
