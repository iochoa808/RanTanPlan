#pragma once

#include "../../encoders/base_encoder.hpp"
#include <z3++.h>
#include <memory>
#include <string>
#include <fstream>

namespace rantanplan {

// Forward declarations
class BaseEncoder;

/**
 * @brief Abstract base class for different propagator strategies
 *
 * Inherits from z3::user_propagator_base so that ALL propagators (including NullPropagator)
 * are Z3-connected. This centralizes:
 *   - Z3 callback registration (fixed, final, decide)
 *   - Solver decision logging (dec, inc, restart events)
 *   - Decision-level tracking for restart detection
 *
 * Subclasses override on_push/on_pop/on_fixed/on_decide/on_final/on_fresh
 * instead of the Z3 virtuals directly.
 */
class PropagatorStrategy : public z3::user_propagator_base {
public:
    PropagatorStrategy(z3::solver& solver, const BaseEncoder& encoder);
    virtual ~PropagatorStrategy() = default;

    // ---- Existing PropagatorStrategy interface (subclasses must implement) ----

    /**
     * @brief Register variables for a specific timestep after constraints are added.
     *        Base implementation emits "inc" when logging and registers fluent variables.
     *        Subclasses should call PropagatorStrategy::register_timestep_variables() first,
     *        then register their own action variables.
     */
    virtual void register_timestep_variables(int timestep);

    virtual void cleanup() = 0;
    virtual std::string get_name() const = 0;
    virtual bool manages_parallelism_constraints() const { return false; }

    // ---- Logging ----

    void enable_logging(const std::string& log_file_path, const std::string& strategy_name);
    bool is_logging_enabled() const { return logging_enabled_; }

    // ---- Z3 callbacks (final — subclasses override on_* instead) ----

    void push() override final;
    void pop(unsigned num_scopes) override final;
    void fixed(z3::expr const& ast, z3::expr const& value) override final;
    void decide(z3::expr const& val, unsigned bit, bool is_pos) override final;
    void final() override final;
    z3::user_propagator_base* fresh(z3::context& ctx) override final;

protected:
    // ---- Virtual hooks for subclasses ----

    virtual void on_push() {}
    virtual void on_pop(unsigned num_scopes) {}
    virtual void on_fixed(z3::expr const& ast, z3::expr const& value) {}
    virtual void on_decide(z3::expr const& val, unsigned bit, bool is_pos) {}
    virtual void on_final() {}
    virtual z3::user_propagator_base* on_fresh(z3::context& ctx) { return nullptr; }

    // ---- Shared state ----

    const BaseEncoder* encoder_;

private:
    // Logging state
    std::ofstream log_file_;
    bool logging_enabled_ = false;
    std::string strategy_name_;

    // Restart detection
    int current_decision_level_ = 0;
    int max_decision_level_seen_ = 0;

    // Register all variables (action + fluent) for logging at a timestep
    void register_all_variables_for_logging(int timestep);
};

} // namespace rantanplan
