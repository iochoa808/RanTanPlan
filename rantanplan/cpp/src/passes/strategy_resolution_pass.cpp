#include "strategy_resolution_pass.hpp"
#include "../config/config.hpp"
#include "../config/strategy_registry.hpp"
#include "../config/strategy_factory.hpp"

namespace rantanplan {

void StrategyResolutionPass::apply(PipelineResult& result) const {
    auto& config = Config::instance();

    StrategySpec spec = StrategyRegistry::get(config.planner.strategy);
    SearchMode mode = parse_search_mode(config.planner.mode);
    spec.planner = resolve_planner_kind(mode, spec);
    StrategyFactory::adjust_spec(spec, result.problem);

    result.resolved_spec = spec;
}

} // namespace rantanplan
