#include "strategy_resolution_pass.hpp"
#include "../config/config.hpp"
#include "../config/strategy_registry.hpp"
#include "../config/strategy_factory.hpp"
#include "../util/logger.hpp"

namespace rantanplan {

void StrategyResolutionPass::apply(PipelineResult& result) const {
    auto& config = Config::instance();

    StrategySpec spec = StrategyRegistry::get(config.planner.strategy);
    SearchMode mode = parse_search_mode(config.planner.mode);
    spec.planner = resolve_planner_kind(mode, spec);
    StrategyFactory::adjust_spec(spec, result.problem);

    // [XTS-UnFun] Only the plain Grounded encoder implements the UF array/set write
    // paths. "uf" is the default, so a chained/R2E strategy would otherwise be
    // unusable without explicitly opting out of a default the caller never chose:
    // downgrade to the one backend those families do implement. An *explicit*
    // --array-encoding uf on such a strategy is a hard error instead, raised earlier
    // in Config::validate. Resolved here, before the pipeline finishes, so that every
    // later reader — the encoder, and recommended_logic in main.cpp/base_planner.cpp —
    // sees the same effective backend.
    if (config.global.array_encoding == "uf" &&
        !config.global.array_encoding_explicit &&
        spec.encoder != EncoderFamily::Grounded) {
        Logger::instance().info(
            "Chained/R2E encoders do not implement the UF array backend — "
            "falling back to --array-encoding theory (default was 'uf').");
        config.global.array_encoding = "theory";
    }

    result.resolved_spec = spec;
}

} // namespace rantanplan
