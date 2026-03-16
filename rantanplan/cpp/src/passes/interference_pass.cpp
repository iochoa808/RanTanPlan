#include "interference_pass.hpp"
#include "../config/strategy_factory.hpp"

namespace rantanplan {

void InterferencePass::apply(PipelineResult& result) const {
    result.interference = StrategyFactory::create_interference(
        result.resolved_spec, result.problem);
}

} // namespace rantanplan
