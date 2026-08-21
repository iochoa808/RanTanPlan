#include "base_planner.hpp"
#include "../config/config.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/stats.hpp"
#include "../util/logger.hpp"

namespace rantanplan {

BasePlanner::BasePlanner(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
    : problem_(problem), encoder_(encoder), ctx_(ctx), solver_(ctx),
      propagator_strategy_(nullptr) {
}

void BasePlanner::set_propagator_strategy(std::unique_ptr<PropagatorStrategy> propagator) {
    propagator_strategy_ = std::move(propagator);

    // ── Z3 solver tuning parameters ────────────────────────────────────
    //
    // CRITICAL: These must be set AFTER the propagator is registered, and
    // must use SMT-level parameter names (not sat.* names).
    //
    // Background (2026-03 investigation):
    //   Z3 has two solver backends with DIFFERENT parameter namespaces:
    //
    //     SAT-level (standalone):  sat.phase, sat.restart, sat.gc, sat.branching.heuristic
    //     SMT-level (with propagators): phase_selection, restart_strategy, lemma_gc_strategy
    //
    //   When a z3::user_propagator_base is registered, Z3 switches to the
    //   SMT backend. Setting sat.* params via solver.set() BEFORE this switch
    //   silently disables ALL user propagator callbacks (fixed, final, push, pop).
    //   The propagator object exists and is registered, but Z3 never invokes it.
    //   This caused invalid plans for every propagator-based strategy (f-p,
    //   fl*, el*, dec) while eager strategies were unaffected
    //   (they encode parallelism as clauses, not via callbacks).
    //
    //   Symptom: the solver returns SAT with interfering actions at the same
    //   timestep (e.g., pick(ball1, right) and pick(ball2, right) in gripper).
    //   Setting sat.* params AFTER the propagator throws "unknown parameter"
    //   because the solver is already in SMT mode at that point.
    auto set_unsat_config = [&](z3::params& p) {
        // restart_strategy: 0=geometric, 1=inner-outer-luby (default), 2=luby, 3=fixed
        p.set("restart_strategy", (unsigned)2);
        p.set("restart_factor", 1.5); // Geometric restart factor (default 1.5)
        // phase_selection: 0=always_false, 1=always_true, 3=caching (default)
        p.set("phase_selection", (unsigned)0);    // Most action vars are false

        // relevancy propagation: we seem to don't want any?
        // https://github.com/Z3Prover/z3/issues/4941
        p.set("smt.relevancy", (unsigned)0);
        //case_split - int
        // 0 - case split based on variable activity,
        // 1 - similar to 0, but delay case splits created during the search,
        // 2 - similar to 0, but cache the relevancy,
        // 3 - case split based on relevancy (structural splitting),
        // 4 - case split on relevancy and activity,
        // 5 - case split on relevancy and current goal,
        // 6 - activity-based case split with theory-aware branching activity
        p.set("smt.case_split", (unsigned)0);

        // controls when Z3 garbage-collects learned lemmas/clauses in the SMT search.
        // 0 = fixed: Z3 deletes inactive lemmas whenever the number of
        //   conflicts since the last lemma-GC exceeds a threshold, using a fixed
        //   interval policy.
        // 1 = geometric: Same trigger style as fixed, but the lemma-GC
        //   threshold grows geometrically over time, so garbage collection
        //   happens less frequently as search progresses.
        // 2 = at restart: Z3 runs lemma cleanup when a restart happens, instead
        //   of on the conflict-threshold schedule.
        // 3 = none: Disables lemma garbage collection.
        p.set("lemma_gc_strategy", (unsigned)1);
    };

    z3::params p(ctx_);
    p.set("random_seed", (uint)42);  // Fixed seed for reproducibility
    //set_unsat_config(p);

    // Set Z3 logic hint based on numeric constraint analysis.
    // This triggers SMT parameter tuning (relevancy, phase
    // selection, restarts, etc.) without changing the arithmetic backend.
    // When the problem is all-integer, use integer logics (QF_IDL/QF_LIA)
    // for tighter integer-specific reasoning.
    // [XTS] has_arrays/uf_arrays select the array/UF logic fragments — this is
    // the call that reaches the solver (main.cpp's identical call only logs).
    const char* logic = recommended_logic(
        arithmetic_profile_, problem_.all_integer(), problem_.has_arrays(),
        Config::instance().global.array_encoding == "uf");
    if (logic) {
        p.set("logic", logic);
    }

    solver_.set(p);
}

std::string BasePlanner::get_propagator_strategy_name() const {
    return propagator_strategy_->get_name();
}

void BasePlanner::init_deadline() {
    auto& config = Config::instance();
    if (config.global.timeout > 0) {
        deadline_ = std::chrono::steady_clock::now() +
                    std::chrono::seconds(config.global.timeout);
    } else {
        // No timeout — set deadline to max so remaining_ms() always returns large value
        deadline_ = std::chrono::steady_clock::time_point::max();
    }
}

unsigned BasePlanner::remaining_ms() const {
    if (deadline_ == std::chrono::steady_clock::time_point::max()) {
        return UINT_MAX;  // No timeout
    }
    auto now = std::chrono::steady_clock::now();
    if (now >= deadline_) return 0;
    return static_cast<unsigned>(
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now).count());
}

bool BasePlanner::apply_solver_timeout(z3::solver& solver) {
    unsigned ms = remaining_ms();
    if (ms == 0) {
        timed_out_ = true;
        Logger::instance().info("\n*** TIMEOUT ***");
        return false;
    }
    if (ms != UINT_MAX) {
        // Only set Z3 timeout when we have a finite deadline
        z3::params p(solver.ctx());
        p.set("timeout", ms);
        solver.set(p);
    }
    return true;
}

bool BasePlanner::handle_unknown_result(const z3::solver& solver, const std::string& context) {
    std::string reason = solver.reason_unknown();
    if (reason.find("timeout") != std::string::npos ||
        reason.find("canceled") != std::string::npos) {
        timed_out_ = true;
        Logger::instance().info("\n*** TIMEOUT during solve at " + context + " ***");
        return true;
    }
    Logger::instance().info("Solver returned unknown at " + context +
                            " (reason: " + reason + ")");
    return false;
}

std::string BasePlanner::format_timeout_string() {
    auto& config = Config::instance();
    return config.global.timeout > 0
        ? std::to_string(config.global.timeout) + "s" : "none";
}

void BasePlanner::collect_statistics() {
    auto& stats = Stats::instance();

    z3::stats z3_stats = get_solver().statistics();
    for (unsigned i = 0; i < z3_stats.size(); ++i) {
        std::string key = "z3." + z3_stats.key(i);

        if (z3_stats.is_uint(i)) {
            stats.set(key, static_cast<double>(z3_stats.uint_value(i)));
        } else if (z3_stats.is_double(i)) {
            stats.set(key, z3_stats.double_value(i));
        }
    }

    stats.set("memory.current_mb", MemoryTracker::instance().get_current_memory_mb());
}

void BasePlanner::add_timestep_constraints(z3::solver& solver, BaseEncoder& encoder,
                                           PropagatorStrategy& propagator, int timestep,
                                           bool prefix_monotone) {
    solver.add(*encoder.encode_actions(timestep));
    solver.add(*encoder.encode_frames(timestep));
    if (prefix_monotone) {
        solver.add(*encoder.encode_prefix_monotone(timestep));
    }
    solver.add(*encoder.encode_symmetries(timestep));
    solver.add(*encoder.encode_state_constraints(timestep));

    if (!propagator.manages_parallelism_constraints()) {
        solver.add(*encoder.encode_parallelism(timestep));
    }

    propagator.register_timestep_variables(timestep + 1);
}

} // namespace rantanplan
