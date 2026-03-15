#include "base_planner.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/stats.hpp"

namespace rantanplan {

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
