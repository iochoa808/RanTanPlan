#pragma once

#include "../problem/problem.hpp"
#include "../encoders/base_encoder.hpp"
#include <z3++.h>
#include <string>

namespace rantanplan {

/**
 * @brief Exports complete SMT formulas for planning problems up to a given timestep
 *
 * The FormulaExporter creates a complete, self-contained SMT-LIB2 formula that includes:
 * - Initial state constraints
 * - Action, frame, parallelism, and symmetry constraints for all timesteps 0..max_timestep
 * - Goal constraints at the final timestep
 *
 * The resulting formula can be fed directly to any SMT solver to find a plan.
 * This is intended for analysis, debugging, and external solving of planning problems.
 */
class FormulaExporter {
public:
    /**
     * @brief Constructor
     * @param problem The planning problem instance
     * @param encoder The encoder to use for generating constraints
     * @param ctx Z3 context (shared)
     */
    FormulaExporter(const Problem& problem, BaseEncoder& encoder, z3::context& ctx);

    /**
     * @brief Export complete formula up to specified timestep
     *
     * Creates a complete SMT-LIB2 formula that includes all constraints necessary
     * to find a plan of length up to max_timestep. The formula is directly solvable.
     *
     * @param max_timestep Maximum timestep to encode (inclusive)
     * @return SMT-LIB2 formatted string of the complete formula
     */
    std::string export_formula(int max_timestep);

private:
    const Problem& problem_;
    BaseEncoder& encoder_;
    z3::context& ctx_;
};

} // namespace rantanplan