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
 * @param initial_result  Pre-populated pipeline result (must have problem set).
 * @param passes          Ordered list of passes to apply (non-owning pointers).
 * @return Accumulated result after all passes (or the first unsolvable one).
 */
PipelineResult run_pipeline(PipelineResult initial_result, const std::vector<const Pass*>& passes);

} // namespace rantanplan
