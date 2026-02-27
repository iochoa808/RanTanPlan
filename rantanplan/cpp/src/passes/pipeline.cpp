#include "pipeline.hpp"
#include "../util/scoped_timer.hpp"

namespace rantanplan {

PipelineResult run_pipeline(Problem initial, const std::vector<const Pass*>& passes) {
    PipelineResult result;
    result.problem = std::move(initial);

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
