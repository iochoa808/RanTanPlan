#include "grounding_pass.hpp"
#include "../grounding/reachability_grounder.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"

namespace rantanplan {

void GroundingPass::apply(PipelineResult& result) const {
    Logger::instance().component(VerbosityLevel::INFO, "Grounding", {{"status", "starting"}});

    ReachabilityGrounder grounder(result.problem);
    GroundingResult gr = grounder.ground();

    // Update pipeline result.
    result.problem = std::move(gr.grounded_problem);

    // Record ground action count in stats for external analysis.
    Stats::instance().set("grounding.ground_actions", static_cast<double>(gr.ground_action_count));

    if (gr.lower_bound > result.lower_bound) {
        result.lower_bound = gr.lower_bound;
    }

    if (gr.proven_unsolvable) {
        result.proven_unsolvable = true;
        result.unsolvable_reason = name();
    }

    Logger::instance().component(VerbosityLevel::INFO, "Grounding", {
        {"complete", std::to_string(gr.ground_action_count) + " actions, " +
                     std::to_string(gr.iterations) + " iterations"}
    });
}

} // namespace rantanplan
