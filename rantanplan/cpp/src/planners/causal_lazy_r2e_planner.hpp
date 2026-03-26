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
 * @brief Causal Lazy R2E planner — LazyR2E with achiever disjunctions
 *
 * Extends the LazyR2E approach with redundant achiever disjunctions that
 * provide the solver with shorter proof paths.  This results in more
 * precise (causal) UNSAT cores: instead of activating all slots that
 * modify a fluent, the solver activates only those that can actually
 * ACHIEVE the required condition (e.g., for goal fuel>=20, only slots
 * whose action increases fuel, not those that decrease it).
 *
 * Two types of achiever disjunctions:
 *
 * 1. Goal achiever disjunctions (assumption-guarded, refreshed):
 *      goal_ach_lit => (act_a_s5 | act_b_s12 | ...)
 *    "At least one achiever of goal condition g must fire."
 *
 * 2. Precondition achiever disjunctions (permanent, per slot):
 *      act_a_sN => (act_b_sM1 | act_c_sM2 | ...)   where M < N
 *    "If action a fires at slot N, at least one preceding achiever of
 *     each precondition must fire (unless init satisfies it)."
 *
 * These clauses are logically redundant (already implied by the chain
 * equations) but provide short-cut proof paths.  The solver naturally
 * prefers shorter proofs, producing smaller, causally-focused cores.
 *
 * AchieversAnalysis (SMT-based, run once at startup) determines which
 * actions can transition each condition from false to true.
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
    };

    /// Assumption-guarded achiever disjunction for a goal condition.
    struct GoalAchieverDisjunction {
        z3::expr lit;             ///< Assumption literal
        bool active;              ///< Include in next check()?
        ExprID condition;         ///< The goal condition this covers
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
    bool achiever_activation_pending_ = false;

    // ---- Achiever data (new) ----

    /// AchieversAnalysis instance (run once at startup).
    std::unique_ptr<AchieversAnalysis> achievers_;

    /// Action::id() → stable const Action* pointer in problem_.actions().
    std::unordered_map<int, const Action*> action_id_to_ptr_;

    /// Condition ExprID → actions that can achieve it (as const Action*).
    std::unordered_map<ExprID, std::vector<const Action*>> condition_achievers_;

    /// Conditions that are already true in the initial state.
    std::unordered_set<ExprID> init_satisfied_conditions_;

    /// Goal achiever disjunctions (assumption-guarded, refreshed with goals).
    std::vector<GoalAchieverDisjunction> goal_achiever_disjunctions_;
    int next_goal_ach_version_ = 0;

    /// Goal condition ExprIDs (from AchieversAnalysis).
    std::vector<ExprID> goal_condition_ids_;

    /// Per-action precondition literal ExprIDs (from AchieversAnalysis).
    std::unordered_map<const Action*, std::vector<ExprID>> action_precondition_ids_;

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

    // ---- Achiever disjunctions (new) ----

    void add_precondition_achiever_disjunctions(size_t slot_idx);
    void refresh_goal_achiever_disjunctions();

    // ---- Goal management ----

    void setup_goal_assumptions();
    void refresh_goal_assumptions();

    // ---- Search loop helpers ----

    z3::expr_vector build_assumptions();
    int process_core(const z3::expr_vector& core);
    int activate_goal_achievers();
    void extend_chain();

    // ---- Plan extraction ----

    Plan extract_plan(const z3::model& model);
};

} // namespace rantanplan
