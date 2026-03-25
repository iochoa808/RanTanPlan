#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "../encoders/base_encoder.hpp"
#include "base_planner.hpp"
#include <z3++.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rantanplan {

/**
 * @brief Lazy R2E planner with flowing frontier
 *
 * Combines R2E chain encoding with core-guided lazy activation.
 *
 * Architecture: one continuous R2E chain growing forward:
 *   [init] → [activated actions] → [blocked frontier] → goal
 *
 * The blocked frontier contains one slot per ground action with full
 * R2E chain equations (chain_var = ite(action_var, effect, prev)).
 * Blocked actions are forced false via assumptions, making them
 * pass-through in the chain.  The solver sees the chain equations
 * and can identify which blocked actions would help via UNSAT cores.
 *
 * When a blocking literal appears in the core:
 *   1. Activate the slot (drop its blocking assumption)
 *   2. Replenish: append a fresh blocked copy at the chain tail
 *   3. Update goal assumptions to reference the new chain tail
 *
 * Chain ordering is by activation order.  Each action's precondition
 * sees the cumulative state of all preceding chain entries, giving
 * valid sequential plan semantics.
 *
 * No interference analysis, propagators, frame axioms, or parallelism
 * constraints — all handled by the R2E chain structure.
 *
 * ── Per-round log line ──────────────────────────────────────────────
 *
 *   [Solving T37] solve: 2.31s | round: 2.31s | slots: 2103 | active: 1420 | blocked: 683 | mem: 379MB
 *
 *   T<N>        Round number (0-based). Each round is one solver check() call.
 *   solve       Wall-clock time for the Z3 check(assumptions) call alone.
 *   round       Wall-clock time for the full round (solve + core processing + replenishment).
 *   slots       Total ActionSlots in the chain (activated + blocked).
 *               Grows when slots are replenished after activation and when the chain is extended.
 *   active      Slots whose blocking assumption has been dropped — the solver is free to set
 *               their action variable to true. These form the "plan capacity" of the chain.
 *   blocked     Slots still forced false via assumptions (= slots − active). These are the
 *               frontier: candidates for activation in future rounds.
 *   mem         Current RSS of the process.
 *
 *   Typical progression: blocked decreases as the solver activates actions through
 *   UNSAT cores, active increases, and slots grows (each activation replenishes one
 *   blocked copy at the tail). When no blocking literal appears in the core and the
 *   current chain capacity is insufficient, the chain is extended (a full new frontier
 *   of blocked slots is appended), causing a jump in both slots and blocked.
 */
class LazyR2EPlanner : public BasePlanner {
public:
    LazyR2EPlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx);

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

    // ---- Data ----

    /// All chain slots in order.
    std::vector<ActionSlot> chain_;
    int next_slot_id_ = 0;

    /// Per-fluent chain tail tracking.
    std::unordered_map<ExprID, VarChainInfo> var_info_;

    /// Goal assumptions (may contain inactive old versions).
    std::vector<GoalAssumption> goal_assumptions_;
    int next_goal_version_ = 0;

    /// ARPG-derived action ordering (computed once, reused for replenishment).
    std::vector<const Action*> action_ordering_;

    /// Fast rank lookup: action pointer → position in action_ordering_.
    std::unordered_map<const Action*, size_t> action_rank_;

    /// Fast core matching: Z3 expr id → chain index.
    std::unordered_map<unsigned, size_t> block_id_to_chain_index_;

    /// Which fluents are modified by at least one action.
    std::unordered_set<ExprID> modifiable_fluents_;

    /// Per-action: which fluents does it modify (grouped by fluent, preserving order).
    std::unordered_map<const Action*,
        std::unordered_map<ExprID, std::vector<const Effect*>>> action_effects_by_fluent_;

    // ---- Setup ----

    void build_action_metadata();
    void compute_action_ordering();

    // ---- Chain building ----

    /// Append a new slot for `action` at the chain tail.
    /// If `blocked`, a blocking assumption is created.
    /// Returns the index in chain_.
    size_t append_slot(const Action* action, bool blocked);

    /// Build the Z3 substitution arrays for the current chain tails.
    /// Maps fluent@0 → chain_tail for every modifiable fluent.
    void build_substitution_arrays(z3::expr_vector& from, z3::expr_vector& to);

    /// Compute the executed value for a group of effects on the same fluent.
    z3::expr compute_effect_value(
        const std::vector<const Effect*>& effects,
        const z3::expr& prev_value,
        const z3::expr_vector& sub_from, const z3::expr_vector& sub_to);

    /// Convert an effect expression value, applying substitution.
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
    void extend_chain();

    // ---- Plan extraction ----

    Plan extract_plan(const z3::model& model);
};

} // namespace rantanplan
