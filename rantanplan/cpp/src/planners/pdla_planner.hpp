#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "../encoders/base_encoder.hpp"
#include "../abstraction/achievers_analysis.hpp"
#include "base_planner.hpp"
#include <z3++.h>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rantanplan {

/**
 * @brief PDLA planner — Property-Directed Lazy Activation
 *
 * Change 1 (float activation): one blocking literal per action.
 * Change 2 (incremental activation): scored selection replaces VSIDS/predictive.
 * Change 3 (obligation-driven search): IC3-style backward chaining.
 *   Phase A: process full obligation queue (no budget — structurally justified).
 *   Phase B: solver check; core → new obligations or fallback direct activation.
 * Phase 4 (split budgets): Phase A unlimited, Phase B fallback capped at K=3.
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

    std::unordered_map<const Action*, z3::expr> block_lit_;
    std::unordered_set<const Action*> blocked_;
    std::unordered_set<const Action*> activated_;
    std::unordered_map<unsigned, const Action*> blk_id_to_action_;

    z3::expr goal_assumption_;
    int next_goal_version_ = 0;

    // ---- Activation budgets ----

    /// Budget for the core-derived direct activation fallback (Phase B).
    /// Kept low because blocking-literal activations are less informed —
    /// the solver says "I want action X" but we don't know the causal reason.
    static constexpr int fallback_budget_ = 3;

    /// Phase A (obligation-driven) has NO budget: obligations are structurally
    /// derived through backward chaining, each addressing a specific missing
    /// condition. The scoring function picks the best achiever. Processing
    /// the full queue in one pass follows the causal chain to completion
    /// before asking the solver to validate — this avoids the round-trip
    /// overhead of artificially limiting to K=3 when per-round solve times
    /// are sub-millisecond (observed: 80 rounds × 0.3ms = 24ms of solver
    /// work gated behind 80 Phase A/B round-trips).

    // ---- Obligation queue (Change 3) ----

    struct Obligation {
        ExprID condition;
        int deadline;
        int depth;
        const Action* requester;  ///< action whose precondition created this (nullptr for goals)
        bool from_core;           ///< true if derived from a solver core (not backward chaining)
    };

    struct ObligationCompare {
        bool operator()(const Obligation& a, const Obligation& b) const {
            if (a.depth != b.depth) return a.depth < b.depth;  // higher depth first
            return a.deadline > b.deadline;  // earlier deadline first
        }
    };

    std::priority_queue<Obligation, std::vector<Obligation>, ObligationCompare> obligation_queue_;
    std::vector<Obligation> deferred_obligations_;

    /// Depth at which each action was activated (for assigning depth to core obligations)
    std::unordered_map<const Action*, int> action_activation_depth_;


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

    // ---- Scoring constants ----

    int max_arpg_layer_ = 1;
    int max_effects_ = 1;

    // ---- Lazy population tracking ----

    std::unordered_map<const Action*, std::unordered_set<int>> action_encoded_at_;

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

    // ---- Search helpers ----

    z3::expr_vector build_assumptions();
    double score_action(const Action* action) const;
    void activate_action(const Action* action);
    void encode_action_at(const Action* action, int timestep);
    bool has_activated_achiever(ExprID condition) const;

    // ---- Obligation-driven search ----

    int process_obligations();
    void push_precondition_obligations(const Action* action, int deadline, int parent_depth);
    struct CoreResult {
        int new_obligations;    ///< obligations pushed (tracked preconds or blocking-lit conditions)
        int horizon_signals;    ///< tracked preconds where ALL achievers already activated (resource exhaustion)
        int direct_activations; ///< actions activated directly from blocking-lit fallback
    };
    CoreResult process_core_for_obligations(const z3::expr_vector& core);

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
