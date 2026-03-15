#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "propagators/propagator_strategy.hpp"
#include <z3++.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

namespace rantanplan {

// ---------------------------------------------------------------------------
// HorizonSchedule
// ---------------------------------------------------------------------------
// Controls which horizons receive a SAT call during planning. Between any two
// consecutive scheduled horizons, ALL transitions are still added to the solver
// (correctness requires a complete transition chain), but only one goal literal
// is placed at the scheduled horizon. Completeness is preserved via the
// prefix-monotone encoding (encode_prefix_monotone) which ensures actions are
// front-loaded so frame axioms propagate shorter plans to the batch horizon.

enum class ScheduleType { LINEAR, ARITHMETIC, GEOMETRIC, DOUBLING };

struct HorizonSchedule {
    ScheduleType type = ScheduleType::LINEAR;

    /**
     * Return the next horizon after `current`, always >= current+1, capped at `max`.
     * Guaranteeing current+1 as a minimum prevents infinite loops when current is 0
     * with doubling (0*2 = 0) or when the factor/step would not advance the index.
     *
     * Per-type hardcoded step/factor:
     *   linear:     +1 per step
     *   arithmetic: +5 per step
     *   geometric:  *1.5 per step
     *   doubling:   *2  per step
     */
    int next(int current, int max) const {
        int n;
        switch (type) {
            case ScheduleType::LINEAR:     n = current + 1;                                      break;
            case ScheduleType::ARITHMETIC: n = current + 5;                                      break;
            case ScheduleType::GEOMETRIC:  n = std::max(current + 1,
                                               static_cast<int>(current * 1.5));                 break;
            case ScheduleType::DOUBLING:   n = std::max(current + 1, current * 2);               break;
            default:                       n = current + 1;                                      break;
        }
        return std::min(n, max);
    }

    static HorizonSchedule from_string(const std::string& name) {
        HorizonSchedule s;
        if      (name == "linear")     s.type = ScheduleType::LINEAR;
        else if (name == "arithmetic") s.type = ScheduleType::ARITHMETIC;
        else if (name == "geometric")  s.type = ScheduleType::GEOMETRIC;
        else if (name == "doubling")   s.type = ScheduleType::DOUBLING;
        else throw std::invalid_argument("Unknown horizon schedule: '" + name +
                                         "'. Valid values: linear, arithmetic, geometric, doubling");
        return s;
    }
};

// ---------------------------------------------------------------------------
// BasePlanner
// ---------------------------------------------------------------------------

/**
 * @brief Base interface for all planner implementations
 *
 * This interface abstracts different planning strategies (e.g., sequential single-ended,
 * double-tail/double-ended). The planner type is part of the strategy configuration,
 * allowing strategies to control both the encoding and the search approach.
 */
class BasePlanner {
public:
    virtual ~BasePlanner() = default;

    /**
     * @brief Main search method - runs the planning algorithm
     * @return Plan object (may be empty if no solution found)
     */
    virtual Plan search() = 0;

    /**
     * @brief Check if the last search found a solution
     * @return true if a valid plan was found, false otherwise
     */
    virtual bool solution_found() const { return solution_found_; }

    /// Whether the planner proved optimality. Default: false (satisficing planners).
    virtual bool optimality_proven() const { return false; }

    /// Best cost found (only meaningful for cost-optimal planners).
    virtual double best_cost() const { return 0.0; }

    /// Whether the search ended due to timeout.
    bool timed_out() const { return timed_out_; }

    /**
     * @brief Set the propagator strategy for this planner
     * @param propagator The propagator strategy to use
     */
    virtual void set_propagator_strategy(std::unique_ptr<PropagatorStrategy> propagator) = 0;

    /**
     * @brief Get the name of the current propagator strategy
     * @return Name of the propagator (e.g., "NullPropagator", "LazyForallPropagator")
     */
    virtual std::string get_propagator_strategy_name() const = 0;

    /**
     * @brief Access to the underlying Z3 solver
     * @return Reference to the Z3 solver used by this planner
     */
    virtual z3::solver& get_solver() = 0;

    /**
     * @brief Set the horizon schedule used during search
     */
    void set_horizon_schedule(HorizonSchedule s) { schedule_ = s; }
    const HorizonSchedule& get_horizon_schedule() const { return schedule_; }

protected:
    /// Horizon schedule; concrete planners initialise this from Config in their constructor.
    HorizonSchedule schedule_;

    bool solution_found_ = false;
    bool timed_out_ = false;

    /// Wall-clock deadline for the search. Set from Config::global::timeout in
    /// init_deadline(), then queried via remaining_ms() before each solver.check().
    std::chrono::steady_clock::time_point deadline_;

    /// Initialise the deadline from Config::global::timeout.
    void init_deadline();

    /// Milliseconds remaining until deadline. Returns 0 if already past.
    unsigned remaining_ms() const;

    /// Apply the remaining timeout to a Z3 solver so that the next check()
    /// call is interrupted if the wall-clock budget runs out.
    /// Returns false if the deadline has already passed (caller should break).
    bool apply_solver_timeout(z3::solver& solver);

    /// Check if a z3::unknown result was caused by timeout. If so, sets
    /// timed_out_ and logs the interruption. Returns true if it was a timeout.
    bool handle_unknown_result(const z3::solver& solver, const std::string& context);

    /// Format the timeout value for log messages (e.g. "120s" or "none").
    static std::string format_timeout_string();

    /// Collect Z3 solver statistics and memory usage into the global Stats singleton.
    /// Uses get_solver() to access the derived class's solver.
    void collect_statistics();

    /// Add standard timestep constraints: actions, frames, symmetries, parallelism,
    /// and propagator variable registration. Optionally includes prefix-monotone
    /// constraints (omitted by DoubleTailPlanner).
    static void add_timestep_constraints(z3::solver& solver, BaseEncoder& encoder,
                                         PropagatorStrategy& propagator, int timestep,
                                         bool prefix_monotone = true);
};

} // namespace rantanplan
