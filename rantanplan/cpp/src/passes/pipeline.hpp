#pragma once

#include "pass.hpp"
#include <vector>

namespace rantanplan {

/**
 * @brief Run a sequence of passes left-to-right.
 *
 * Threads a PipelineResult through each pass. Short-circuits if any pass
 * proves the problem unsolvable.
 *
 * @param initial   The input problem.
 * @param passes    Ordered list of passes to apply (non-owning pointers).
 * @return Accumulated result after all passes (or the first unsolvable one).
 */
PipelineResult run_pipeline(Problem initial, const std::vector<const Pass*>& passes);

} // namespace rantanplan
