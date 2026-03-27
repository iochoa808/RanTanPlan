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
 * @brief Causal Lazy R2E planner — LazyR2E with achiever-based core filtering
 *
 * Extends LazyR2E with causal UNSAT core filtering: AchieversAnalysis
 * (SMT-based, run once at startup) determines which actions can
 * transition each condition from false to true.  A transitive closure
 * from goal conditions through the achiever graph identifies
 * "goal-relevant" actions.  Blocking literals for non-relevant actions
 * (e.g. self-loop flights) are filtered from UNSAT cores, preventing
 * the planner from activating useless slots.
 *
 * Additionally uses VSIDS-inspired multiplicity tracking: core hits
 * bump action scores, cascade propagation bumps enablers, and global
 * decay ensures old evidence fades.  When an action's score exceeds
 * its active slot count, new slots are threshold-activated.
 */
class CausalLazyR2EPlanner : public BasePlanner {
public:
    CausalLazyR2EPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx);

    Plan search() override;

private:
    /// A slot in the R2E chain: one (action, position) pair.
    struct ActionSlot {
        const Action* action;
        int slot_id;              ///< Unique slot identifier
        bool activated;           ///< false = blocked by assumption
        z3::expr action_var;      ///< The Z3 action boolean
        z3::expr blocking_lit;    ///< Assume this to block the action
        /// Chain variables: chain_vars[fluent_id] = chain var for this slot
        std::unordered_map<ExprID, z3::expr> chain_vars;
    };

    /// Per-variable tracking of the chain tail.
    struct VarChainInfo {
        z3::expr chain_tail;      ///< Chain var of the last modifier (or fluent@0)
    };

    /// A goal assumption: assumption-guarded goal clause.
    struct GoalAssumption {
        z3::expr lit;             ///< The assumption literal
        bool active;              ///< Include in next check()?
        ExprID goal_id;           ///< The goal ExprID this assumption guards
    };

    // ---- Chain data (same as LazyR2EPlanner) ----

    std::vector<ActionSlot> chain_;
    int next_slot_id_ = 0;
    std::unordered_map<ExprID, VarChainInfo> var_info_;
    std::vector<GoalAssumption> goal_assumptions_;
    int next_goal_version_ = 0;
    std::vector<const Action*> action_ordering_;
    std::unordered_map<const Action*, size_t> action_rank_;
    std::unordered_map<unsigned, size_t> block_id_to_chain_index_;
    std::unordered_set<ExprID> modifiable_fluents_;
    std::unordered_map<const Action*,
        std::unordered_map<ExprID, std::vector<const Effect*>>> action_effects_by_fluent_;
    // ---- Achiever data (new) ----

    /// AchieversAnalysis instance (run once at startup).
    std::unique_ptr<AchieversAnalysis> achievers_;

    /// Action::id() → stable const Action* pointer in problem_.actions().
    std::unordered_map<int, const Action*> action_id_to_ptr_;

    /// Condition ExprID → actions that can achieve it (as const Action*).
    std::unordered_map<ExprID, std::vector<const Action*>> condition_achievers_;

    /// Conditions that are already true in the initial state.
    std::unordered_set<ExprID> init_satisfied_conditions_;

    /// Goal condition ExprIDs (from AchieversAnalysis).
    std::vector<ExprID> goal_condition_ids_;

    /// Per-action precondition literal ExprIDs (from AchieversAnalysis).
    std::unordered_map<const Action*, std::vector<ExprID>> action_precondition_ids_;

    /// Actions that are goal achievers (computed once at init).
    std::unordered_set<const Action*> goal_achiever_actions_;

    /// Actions that are transitive achievers of any goal condition
    /// (computed once at init).  Used to filter non-achiever noise from
    /// UNSAT cores — blocking lits for actions outside this set are
    /// ignored since they can never contribute to goal achievement.
    std::unordered_set<const Action*> goal_relevant_actions_;

    // ---- VSIDS-inspired multiplicity tracking ----

    /// Activity score per action type (VSIDS-style).  Bumped on core
    /// hits and cascade propagation.  Globally decayed each round so
    /// that old evidence fades and only actions with fresh support
    /// maintain high scores.  When floor(activity) exceeds active_count,
    /// new slots are activated via threshold_activate().
    std::unordered_map<const Action*, double> multiplicity_;

    /// Current number of active slots per action type.
    std::unordered_map<const Action*, int> active_count_;

    /// VSIDS decay factor applied to ALL multiplicity scores each round.
    /// Recent core/cascade evidence dominates; old evidence fades.
    static constexpr double vsids_decay_ = 0.95;

    // ---- Setup ----

    void build_action_metadata();
    void compute_action_ordering();

    // ---- Achiever setup (new) ----

    void initialize_achievers();
    void build_action_id_map();
    void build_condition_achiever_cache();
    void compute_init_satisfied_conditions();

    // ---- Chain building ----

    size_t append_slot(const Action* action, bool blocked);
    void build_substitution_arrays(z3::expr_vector& from, z3::expr_vector& to);
    z3::expr compute_effect_value(
        const std::vector<const Effect*>& effects,
        const z3::expr& prev_value,
        const z3::expr_vector& sub_from, const z3::expr_vector& sub_to);
    z3::expr create_effect_value_z3(
        const EffectExpression& eff_expr,
        const z3::expr& running_value,
        const z3::expr_vector& sub_from, const z3::expr_vector& sub_to);

    // ---- Goal management ----

    void setup_goal_assumptions();
    void refresh_goal_assumptions();

    // ---- Search loop helpers ----

    z3::expr_vector build_assumptions();
    int process_core(const z3::expr_vector& core);
    void cascade_bump(const std::vector<const Action*>& seeds, double bump_amount);
    int threshold_activate();
    void decay_multiplicity();
    void extend_chain();

    // ---- Plan extraction ----

    Plan extract_plan(const z3::model& model);
};

} // namespace rantanplan
