#include "formula_exporter.hpp"
#include "../util/stats.hpp"
#include <iostream>

namespace rantanplan {

FormulaExporter::FormulaExporter(const Problem& problem, BaseEncoder& encoder, z3::context& ctx)
    : problem_(problem), encoder_(encoder), ctx_(ctx) {
}

std::string FormulaExporter::export_formula(int max_timestep) {
    z3::solver formula_solver(ctx_);
    auto& stats = Stats::instance();

    if (max_timestep < 0) {
        std::cerr << "Error: max_timestep must be non-negative" << std::endl;
        return "";
    }

    // Add initial state constraints (timestep 0)
    auto initial_state = encoder_.encode_initial_state();
    if (initial_state) {
        formula_solver.add(*initial_state);
    }

    // Add transition constraints for each timestep
    for (int t = 0; t <= max_timestep; t++) {
        // Add action constraints
        auto actions = encoder_.encode_actions(t);
        if (actions) {
            formula_solver.add(*actions);
        }

        // Add frame axioms (for transitions t -> t+1, so skip the last timestep)
        if (t < max_timestep) {
            auto frames = encoder_.encode_frames(t);
            if (frames) {
                formula_solver.add(*frames);
            }
        }

        // Add parallelism constraints
        auto parallelism = encoder_.encode_parallelism(t);
        if (parallelism) {
            formula_solver.add(*parallelism);
        }

        // Add symmetry constraints
        auto symmetries = encoder_.encode_symmetries(t);
        if (symmetries) {
            formula_solver.add(*symmetries);
        }
    }

    // Add goal constraints at the final timestep
    auto goal = encoder_.encode_goal(max_timestep);
    if (goal) {
        formula_solver.add(*goal);
    }

    // Export the complete formula as SMT-LIB2
    std::string smt2_formula = formula_solver.to_smt2();

    // Collect some basic statistics
    stats.set("formula_exporter.max_timestep", max_timestep);
    stats.set("formula_exporter.num_assertions", formula_solver.assertions().size());

    return smt2_formula;
}

} // namespace rantanplan