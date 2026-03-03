#include "grounding_pass.hpp"
#include "../grounding/reachability_grounder.hpp"
#include "../util/logger.hpp"

namespace rantanplan {

void GroundingPass::apply(PipelineResult& result) const {
    Logger::instance().info("Running reachability grounding...");

    ReachabilityGrounder grounder(result.problem);
    GroundingResult gr = grounder.ground();

    // Update pipeline result.
    result.problem = std::move(gr.grounded_problem);

    if (gr.lower_bound > result.lower_bound) {
        result.lower_bound = gr.lower_bound;
    }

    if (gr.proven_unsolvable) {
        result.proven_unsolvable = true;
        result.unsolvable_reason = name();
    }

    Logger::instance().info("Grounding pass complete: " +
                  std::to_string(gr.ground_action_count) + " ground actions, " +
                  std::to_string(gr.iterations) + " iterations");
}

} // namespace rantanplan
