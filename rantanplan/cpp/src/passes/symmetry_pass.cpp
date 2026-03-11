#include "symmetry_pass.hpp"
#include "../symmetries/smt_symmetry_checker.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include <z3++.h>

namespace rantanplan {

void SymmetryDetectionPass::apply(PipelineResult& result) const {
    z3::context ctx;
    SMTSymmetryChecker checker(&result.problem, ctx);
    result.detected_object_swaps = checker.detect_all_object_swaps();

    Logger::instance().info("Symmetry detection: " +
                            std::to_string(result.detected_object_swaps.size()) + " symmetric pairs");
    Stats::instance().set("symmetries.pairs",
                          static_cast<double>(result.detected_object_swaps.size()));
}

void SymmetryCompletionPass::apply(PipelineResult& result) const {
    if (result.detected_object_swaps.empty()) return;

    // Use a throwaway Z3 context — compute_symmetry_pairs doesn't need SMT
    z3::context ctx;
    SMTSymmetryChecker checker(&result.problem, ctx);
    checker.compute_symmetry_pairs(result.detected_object_swaps);

    result.symmetry_data = checker.get_all_symmetries();
}

} // namespace rantanplan
