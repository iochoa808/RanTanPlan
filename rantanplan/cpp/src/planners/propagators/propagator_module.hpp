#pragma once

#include "../../problem/problem.hpp"
#include "../../encoders/z3_variable_factory.hpp"
#include "../../encoders/base_encoder.hpp"
#include "../../encoders/grounded_encoder.hpp"
#include "../../analysis/interference_analysis.hpp"
#include "../../problem/visitors/fluent_collector.hpp"
#include <z3++.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <string>

namespace rantanplan {

class PropagatorStrategy;

// ---------------------------------------------------------------------------
// Shared state — owned by CompositePropagator, accessed by all modules.
// ---------------------------------------------------------------------------

struct PropagatorSharedState {
    // External references (set once during initialization)
    const Problem* problem = nullptr;
    const Z3VariableFactory* variable_factory = nullptr;
    const ParallelismStrategy* parallelism = nullptr;
    const InterferenceAnalysis* interference = nullptr;
    BaseEncoder* encoder = nullptr;
    z3::solver* solver = nullptr;

    // Action trail (push/pop managed by CompositePropagator)
    std::vector<std::pair<int, int>> action_trail;     // (action_id, timestep)
    std::vector<size_t> action_decision_levels;
    std::unordered_map<int, std::unordered_set<int>> active_actions_per_timestep;

    // Registered action variables (shared; modules should check before re-registering)
    std::unordered_map<int, std::vector<std::shared_ptr<z3::expr>>> registered_action_vars;

    // Footprint index — built once, shared read-only
    std::unordered_map<int, std::vector<int>> potential_interferers;
    bool footprint_index_built = false;

    void build_footprint_index() {
        if (footprint_index_built) return;
        footprint_index_built = true;

        std::unordered_map<ExprID, std::vector<int>> fluent_to_actions;
        for (const Action& action : problem->actions()) {
            int aid = action.id();
            for (const auto& effect : action.effects())
                fluent_to_actions[effect.effect_expression().fluent_id()].push_back(aid);
            if (action.has_precondition()) {
                FluentCollector collector(*problem);
                collector.collect_from_id(action.precondition_id());
                for (ExprID fid : collector.get_fluents())
                    fluent_to_actions[fid].push_back(aid);
            }
        }
        for (const auto& [fid, aids] : fluent_to_actions)
            for (size_t i = 0; i < aids.size(); ++i)
                for (size_t j = i + 1; j < aids.size(); ++j)
                    if (aids[i] != aids[j]) {
                        potential_interferers[aids[i]].push_back(aids[j]);
                        potential_interferers[aids[j]].push_back(aids[i]);
                    }
        for (auto& [aid, nb] : potential_interferers) {
            std::sort(nb.begin(), nb.end());
            nb.erase(std::unique(nb.begin(), nb.end()), nb.end());
        }
    }
};

// ---------------------------------------------------------------------------
// Module interface
// ---------------------------------------------------------------------------

class PropagatorModule {
public:
    virtual ~PropagatorModule() = default;

    /// Called once after construction. Stores references for later use.
    virtual void initialize(PropagatorSharedState& shared,
                            PropagatorStrategy& host) {
        shared_ = &shared;
        host_ = &host;
    }

    /// Register Z3 variables for a new timestep.
    virtual void register_timestep_variables(int /*timestep*/) {}

    /// Z3 callbacks (dispatched in sequence by PropagatorStrategy).
    virtual void on_push() {}
    virtual void on_pop(unsigned /*num_scopes*/) {}
    virtual void on_fixed(const z3::expr& /*ast*/, const z3::expr& /*value*/) {}
    virtual void on_decide(const z3::expr& /*val*/, unsigned /*bit*/, bool /*is_pos*/) {}
    virtual void on_final() {}

    /// Whether this module needs Z3's decide callback.
    /// Only modules returning true cause the (expensive) callback to be registered.
    virtual bool wants_decide_callback() const { return false; }

    /// PDLA hook — called between solver.check() calls when an action is activated.
    virtual void on_action_activated(int /*action_id*/, int /*max_timestep*/) {}

    /// Serialize parallel actions at a timestep into a valid execution order.
    /// Returns empty vector if this module has no opinion (use default).
    virtual std::vector<const Action*> serialize_actions(
        int /*timestep*/, const std::vector<const Action*>& /*actions*/) const {
        return {};
    }

    /// Report statistics at end of search.
    virtual void cleanup() {}

    /// Whether this module manages parallelism constraints (mutex / cycle).
    virtual bool manages_parallelism_constraints() const { return false; }

    /// Human-readable name for logging.
    virtual std::string get_name() const = 0;

protected:
    PropagatorSharedState* shared_ = nullptr;
    PropagatorStrategy* host_ = nullptr;
};

} // namespace rantanplan
