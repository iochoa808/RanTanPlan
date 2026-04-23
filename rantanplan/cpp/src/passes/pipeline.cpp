#include "pipeline.hpp"
#include "../analysis/interference_analysis.hpp"
#include "../analysis/numeric_relaxed_planning_graph.hpp"
#include "../abstraction/achievers_analysis.hpp"
#include "../util/scoped_timer.hpp"

namespace rantanplan {

PipelineResult run_pipeline(PipelineResult initial_result, const std::vector<const Pass*>& passes) {
    PipelineResult result = std::move(initial_result);

    for (const auto* pass : passes) {
        ScopedTimer timer("pass." + pass->name() + ".time_ms");

        pass->apply(result);

        if (result.proven_unsolvable) {
            break;
        }
    }

    return result;
}

} // namespace rantanplan
