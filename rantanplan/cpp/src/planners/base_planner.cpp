#include "base_planner.hpp"
#include "../config/config.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/stats.hpp"
#include "../util/logger.hpp"

namespace rantanplan {

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

    if (!propagator.manages_parallelism_constraints()) {
        solver.add(*encoder.encode_parallelism(timestep));
    }

    propagator.register_timestep_variables(timestep + 1);
}

} // namespace rantanplan
