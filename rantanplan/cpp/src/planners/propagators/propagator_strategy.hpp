#pragma once

#include "propagator_module.hpp"
#include <z3++.h>
#include <vector>
#include <memory>
#include <string>

namespace rantanplan {

/**
 * @brief The single Z3 user propagator that dispatches to composable modules.
 *
 * Z3 supports exactly one user_propagator_base per solver.  PropagatorStrategy
 * owns that slot and multiplexes callbacks to an ordered list of modules.
 *
 * Shared state (action trail, footprint index, registered vars) lives in
 * PropagatorSharedState, managed here.  Each module maintains its own
 * private trail for its own concern (frame axioms, edge literals, etc.).
 *
 * Conflict short-circuit: when any module raises a conflict(), subsequent
 * modules are skipped for the remainder of that callback invocation.
 * Propagations (unit implications) do NOT trigger the short-circuit.
 */
class PropagatorStrategy : public z3::user_propagator_base {
public:
    PropagatorStrategy(z3::solver& solver, const Problem& problem,
                       BaseEncoder& encoder);
    ~PropagatorStrategy() override = default;

    /// Add a module. Modules are called in insertion order.
    /// Call BEFORE the first solver.check().
    void add_module(std::unique_ptr<PropagatorModule> m);

    // --- Z3 API delegation for modules ---

    /// Modules call this instead of conflict() directly.
    void module_conflict(const z3::expr_vector& fixed);

    /// Modules call this instead of propagate() directly.
    void module_propagate(const z3::expr_vector& fixed, const z3::expr& consequence);

    /// Modules call this instead of add() directly.
    void module_add(const z3::expr& e);

    /// Register the Z3 decide callback.  Called by modules that need
    /// on_decide notifications (e.g. logging).  Safe to call multiple times;
    /// the Z3 registration happens at most once.
    void register_decide_callback();

    /// Check whether a conflict has been raised in the current callback.
    bool conflict_raised() const { return conflict_raised_; }

    /// Access the Z3 context (modules need this to build expressions).
    z3::context& z3_ctx() { return ctx(); }

    // --- External interface (called by planners) ---

    void register_timestep_variables(int timestep);
    void cleanup();
    std::string get_name() const;
    bool manages_parallelism_constraints() const;
    void on_action_activated(int action_id, int max_timestep);
    std::vector<const Action*> serialize_actions(
        int timestep, const std::vector<const Action*>& actions) const;

    PropagatorSharedState& shared() { return shared_; }

    // --- Z3 callbacks (final overrides) ---

    void push() override final;
    void pop(unsigned num_scopes) override final;
    void fixed(z3::expr const& ast, z3::expr const& value) override final;
    void decide(z3::expr const& val, unsigned bit, bool is_pos) override final;
    void final() override final;
    z3::user_propagator_base* fresh(z3::context& ctx) override final;

private:
    PropagatorSharedState shared_;
    std::vector<std::unique_ptr<PropagatorModule>> modules_;
    bool conflict_raised_ = false;
    bool decide_registered_ = false;

    /// Update the shared action trail when an action variable is fixed to true.
    bool update_action_trail(const z3::expr& ast, const z3::expr& value);
};

} // namespace rantanplan
