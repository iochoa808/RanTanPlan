#include "symmetry_pass.hpp"
#include "../symmetries/smt_symmetry_checker.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include <z3++.h>

namespace rantanplan {

void SymmetryPass::apply(PipelineResult& result) const {
    z3::context ctx;
    SMTSymmetryChecker checker(&result.problem, ctx);
    checker.detect_all_object_swaps();

    result.symmetry_data = checker.get_all_symmetries();

    Logger::instance().info("Symmetry detection: " +
                            std::to_string(result.symmetry_data.size()) + " symmetric pairs");
    Stats::instance().set("symmetries.pairs", static_cast<double>(result.symmetry_data.size()));
}

} // namespace rantanplan
