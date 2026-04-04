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
 * Core-guided lazy activation for exists-step SAT-based planning,
 * structured as a two-phase loop inspired by IC3/PDR and CEGAR.
 *
 * Architecture:
 *   - Float activation: one blocking literal per action (not per timestep).
 *     Activating an action makes it available at all timesteps.
 *   - Obligation queue: backward chaining from goals through the achiever
 *     graph.  Obligations are processed without budget (exploitation).
 *   - Core classification: tracked preconditions are classified into
 *     need (no achiever), alternative (try different ground instance,
 *     K=1 per round — exploration), or horizon signal (resource exhaustion).
 *   - Spread priority: core-derived obligations are prioritised by the
 *     fraction of timesteps at which the condition fails — low spread
 *     (likely to benefit from alternatives) before high spread.
 *   - Assumption shuffling: blocking literals are shuffled each round
 *     to decorrelate Z3's search heuristics from assumption ordering.
 *
 * Two variants (controlled by always_activate_core_):
 *   - pdla (always_activate_core_=true): blocking literals from cores
 *     are always activated alongside structured obligation processing.
 *     Self-regulating: sparse domains stay lean (few blocking lits in
 *     cores), dense domains converge fast (many blocking lits activated).
 *   - pdla-sel (always_activate_core_=false): blocking literals are
 *     only activated as fallback when structured processing produces
 *     no obligations.  More selective, better for sparse domains,
 *     slower on dense domains where most actions are eventually needed.
 *
 * See docs/pdla-proposal.md for the full design document.
 */
class PDLAPlanner : public BasePlanner {
public:
    PDLAPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx);
    ~PDLAPlanner() override = default;

    void set_achievers(std::unique_ptr<AchieversAnalysis> achievers);

    /// When true, blocking literals from cores are always activated
    /// (the solver's direct demand is trusted unconditionally).
    /// When false, blocking literals are only activated as a fallback
    /// when structured processing produces no obligations.
    void set_always_activate_core(bool v) { always_activate_core_ = v; }

    Plan search() override;

protected:
    // ---- Timestep + blocking management (float activation) ----

    int current_horizon_ = -1;
    int goal_timestep_ = -1;
    bool always_activate_core_ = true;

    std::unordered_map<const Action*, z3::expr> block_lit_;
    std::unordered_set<const Action*> blocked_;
    std::unordered_set<const Action*> activated_;
    std::unordered_map<unsigned, const Action*> blk_id_to_action_;

    z3::expr goal_assumption_;
    int next_goal_version_ = 0;

    // ---- Activation budgets ----
    //
    // Phase A backward chain (from_core=false): unlimited.  These are
    // committed causal-chain obligations — exploitation.
    //
    // Phase A core alternatives (from_core=true): K=1.  One refinement
    // per counterexample — exploration, IC3/CEGAR discipline.
    //
    // Phase B direct fallback (blocking literals, no structured info):
    // ALL.  When the three-way classification and obligation conversion
    // produce nothing, we have no basis for selectivity.  Activating all
    // blocking literals from the core lets PDLA self-regulate: sparse
    // domains stay lean (fallback rarely fires), dense domains converge
    // to CE-like bulk activation (fallback handles most rounds).

    // ---- Obligation queue (Change 3) ----

    struct Obligation {
        ExprID condition;
        int deadline;
        int depth;
        const Action* requester;  ///< action whose precondition created this (nullptr for goals)
        bool from_core;           ///< true if derived from a solver core (not backward chaining)
        double spread;            ///< fraction of timesteps at which condition fails in core [0,1]
    };

    struct ObligationCompare {
        bool operator()(const Obligation& a, const Obligation& b) const {
            // 1. Higher depth first (deeper causal chains resolved first)
            if (a.depth != b.depth) return a.depth < b.depth;
            // 2. Lower spread first (conditions failing at fewer timesteps
            //    are more likely to benefit from alternatives — the solver
            //    only proved failure at a subset of temporal placements)
            if (a.spread != b.spread) return a.spread > b.spread;
            // 3. Earlier deadline first
            return a.deadline > b.deadline;
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
