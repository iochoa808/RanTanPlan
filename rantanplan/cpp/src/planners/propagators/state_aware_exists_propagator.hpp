#pragma once

#include "propagator_strategy.hpp"
#include "../../problem/problem.hpp"
#include "../../problem/action.hpp"
#include "../../analysis/semantic_interference_analysis.hpp"
#include "../../util/hash_utils.hpp"
#include <z3++.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rantanplan {

/**
 * @brief Edge-triggered state-aware exists propagator (PDLA only).
 *
 * Detects interference cycles with state-dependent edge conditions.
 * Edge literals are registered at action activation time (PDLA's
 * on_action_activated, between solver.check() calls).  Z3's theory solver
 * determines each edge literal's value from the interference condition.
 *
 * === Three-case edge classification ===
 *   NEVER:     condition always false → no edge, no literal.
 *   ALWAYS:    condition always true  → static edge, no literal.
 *   SOMETIMES: condition contingent   → Z3 edge literal registered.
 *
 * === Edge-triggered cycle detection ===
 *   on_fixed(action_var, true): add to active set, DFS with PRESENT-only.
 *   on_fixed(edge_lit, true):   check for cycles, propagate/conflict with
 *                                edge literals in justification.
 *   on_final():                 soundness backstop (UNKNOWN=PRESENT).
 *
 * Justifications include edge literals → no stale clauses on backtrack.
 * Dynamic edge set ⊆ Static edge set.
 */
class StateAwareExistsPropagator : public PropagatorStrategy {
public:
    StateAwareExistsPropagator(z3::solver& solver, const Problem& problem,
                               BaseEncoder& encoder);
    ~StateAwareExistsPropagator() override = default;

    void on_push() override;
    void on_pop(unsigned num_scopes) override;
    void on_fixed(z3::expr const& ast, z3::expr const& value) override;
    void on_final() override;

    void register_timestep_variables(int timestep) override;
    void cleanup() override;
    std::string get_name() const override { return "StateAwareExistsPropagator"; }
    bool manages_parallelism_constraints() const override { return true; }

    void on_action_activated(int action_id, int max_timestep) override;

private:
    const Problem* problem_;
    const Z3VariableFactory* variable_factory_;
    const ParallelismStrategy* parallelism_strategy_;
    const InterferenceAnalysis* interference_analyzer_;
    BaseEncoder* encoder_nc_;
    z3::solver* solver_ptr_;

    // --- Action trail ---
    std::vector<std::pair<int, int>> trail_;
    std::vector<size_t> decision_levels_;
    std::unordered_map<int, std::unordered_set<int>> active_actions_per_timestep_;
    std::unordered_map<int, std::vector<std::shared_ptr<z3::expr>>> registered_action_vars_;

    // --- Stats ---
    int cycle_count_ = 0;
    int edges_skipped_ = 0;
    int two_cycle_propagations_ = 0;
    int on_final_cycles_ = 0;

    // --- Footprint index ---
    std::unordered_map<int, std::vector<int>> potential_interferers_;
    bool footprint_index_built_ = false;
    void build_footprint_index();

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

    // --- Condition formula ---
    z3::expr build_condition_z3(const Action& source, const Action& target,
                                int timestep);
    z3::expr build_check1_z3(const Action& source, const Action& target,
                              int timestep,
                              const z3::expr_vector& from_s,
                              const z3::expr_vector& to_s);
    z3::expr build_check2_z3(const Action& source, const Action& target,
                              int timestep,
                              const z3::expr_vector& from_s,
                              const z3::expr_vector& to_s);
    bool build_effect_substitution(const Action& action, int timestep,
                                   z3::expr_vector& from, z3::expr_vector& to);

    /// Create edge literal AND link to condition at a single timestep.
    /// Only for SOMETIMES pairs. Called from on_action_activated (safe timing).
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
