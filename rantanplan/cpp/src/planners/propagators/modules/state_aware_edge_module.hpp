#pragma once

#include "../propagator_module.hpp"
#include "../propagator_strategy.hpp"
#include "../../../analysis/semantic_interference_analysis.hpp"
#include "../../../util/hash_utils.hpp"
#include <z3++.h>

namespace rantanplan {

/**
 * @brief State-aware edge module (PDLA only).
 *
 * Classifies interference edges as NEVER/ALWAYS/SOMETIMES at action
 * activation time. SOMETIMES edges get Z3 literals whose truth value
 * depends on a state-dependent interference condition.
 *
 * Edge-triggered cycle detection:
 *   on_fixed(action_var, true) → DFS with PRESENT-only edges.
 *   on_fixed(edge_lit, true)   → check for cycles via edges.
 *   on_final()                 → soundness backstop (UNKNOWN=PRESENT).
 *
 * Owns its own edge trail for push/pop.
 * Reads shared action trail for active action set.
 */
class StateAwareEdgeModule : public PropagatorModule {
public:
    std::string get_name() const override { return "StateAwareEdge"; }
    bool manages_parallelism_constraints() const override { return true; }

    void on_push() override;
    void on_pop(unsigned num_scopes) override;
    void on_fixed(const z3::expr& ast, const z3::expr& value) override;
    void on_final() override;
    void on_action_activated(int action_id, int max_timestep) override;
    void register_timestep_variables(int timestep) override;
    void cleanup() override;

    /// Serialize parallel actions into a valid execution order using the
    /// realized edge graph from the Z3 model.
    ///
    /// The standard plan extraction path (encoder → topological_sort_actions)
    /// uses the static interference analysis, which may disagree with the
    /// state-aware edge classification.  This method reads edge literal
    /// values directly from the SAT model, producing a serialization that
    /// is consistent with what Z3 actually proved.  For pairs without edge
    /// literals (classified NEVER), the static analysis is used as fallback.
    std::vector<const Action*> serialize_actions(
        int timestep, const std::vector<const Action*>& actions) const override;

private:
    // --- Edge literal tracking ---
    enum class EdgeStatus : int8_t { UNKNOWN = 0, PRESENT = 1, ABSENT = 2 };

    struct EdgeKey {
        int src, tgt, timestep;
        bool operator==(const EdgeKey& o) const {
            return src == o.src && tgt == o.tgt && timestep == o.timestep;
        }
    };
    struct EdgeKeyHash {
        size_t operator()(const EdgeKey& k) const {
            size_t h = std::hash<int>{}(k.src);
            h ^= std::hash<int>{}(k.tgt) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>{}(k.timestep) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::unordered_map<EdgeKey, EdgeStatus, EdgeKeyHash> edge_status_;
    std::unordered_map<EdgeKey, z3::expr, EdgeKeyHash> edge_key_to_lit_;
    struct EdgeInfo { int src, tgt, timestep; };
    std::unordered_map<unsigned, EdgeInfo> edge_lit_to_info_;

    struct EdgeTrailEntry { EdgeKey key; EdgeStatus prev_status; };
    std::vector<EdgeTrailEntry> edge_trail_;
    std::vector<size_t> edge_decision_levels_;

    std::unordered_set<std::pair<int,int>, PairHash> registered_pairs_;
    std::unordered_set<int> activated_action_ids_;
    int current_max_timestep_ = -1;

    // --- Edge classification (cached) ---
    enum class EdgeClass { UNCHECKED, NEVER, ALWAYS, SOMETIMES };
    std::unordered_map<std::pair<int,int>, EdgeClass, PairHash> edge_class_cache_;
    EdgeClass classify_edge(int src_id, int tgt_id);

    // --- Stats ---
    int cycle_count_ = 0;
    int edges_skipped_ = 0;
    int two_cycle_propagations_ = 0;
    int on_final_cycles_ = 0;

    // --- Condition formula construction ---
    //
    // These methods build a Z3 formula that expresses when one action's effects
    // interfere with another's preconditions or effects.  They duplicate logic
    // that also exists in SemanticInterferenceAnalysis, but with a key difference:
    // the semantic analysis computes interference ONCE at preprocessing time
    // (state-independent, over-approximate), while these formulas are
    // state-DEPENDENT — they use timestep-specific Z3 variables so that Z3
    // evaluates them under the actual model assignment.
    //
    // This duplication exists because the interference analysis was designed
    // as a static preprocessing step, not as something that produces Z3
    // expressions embeddable in the solver.  A future refactor could unify
    // these by having the analysis produce reusable Z3 formula templates.
    z3::expr build_condition_z3(const Action& source, const Action& target, int timestep);
    z3::expr build_check1_z3(const Action& source, const Action& target, int timestep,
                              const z3::expr_vector& from_s, const z3::expr_vector& to_s);
    z3::expr build_check2_z3(const Action& source, const Action& target, int timestep,
                              const z3::expr_vector& from_s, const z3::expr_vector& to_s);
    bool build_effect_substitution(const Action& action, int timestep,
                                   z3::expr_vector& from, z3::expr_vector& to);

    void register_edge_at_timestep(int src_id, int tgt_id, int timestep);

    // --- Cycle detection ---
    enum class EdgeFilter { PRESENT_ONLY, PRESENT_AND_UNKNOWN };
    void handle_edge_present(int src, int tgt, int timestep);
    void report_cycle(const std::vector<int>& cycle, int timestep);
    bool find_cycle_in_active_actions(const std::unordered_set<int>& active,
                                      int timestep, std::vector<int>& cycle,
                                      EdgeFilter filter);
};

} // namespace rantanplan
