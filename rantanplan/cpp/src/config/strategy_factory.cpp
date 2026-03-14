#include "strategy_factory.hpp"

#include "../encoders/grounded_encoder.hpp"
#include "../encoders/chained_grounded_encoder.hpp"
#include "../encoders/r2e_grounded_encoder.hpp"

#include "../encoders/parallelism/sequential_semantics.hpp"
#include "../encoders/parallelism/forall_semantics.hpp"
#include "../encoders/parallelism/exists_semantics.hpp"

#include "../encoders/parallelism/eager_interference_analysis.hpp"
#include "../encoders/parallelism/eager_semantic_interference_analysis.hpp"
#include "../encoders/parallelism/lazy_interference_analysis.hpp"
#include "../encoders/parallelism/semantic_interference_analysis.hpp"

#include "../planners/sequential.hpp"
#include "../planners/double_tail_planner.hpp"

#include "../planners/propagators/null_propagator.hpp"
#include "../planners/propagators/forall_propagator.hpp"
#include "../planners/propagators/lazy_forall_propagator.hpp"
#include "../planners/propagators/exists_propagator.hpp"
#include "../planners/propagators/decision_heuristic_propagator.hpp"

#include <stdexcept>

namespace rantanplan {

static bool is_lazy(InterferenceKind kind) {
    return kind == InterferenceKind::LazySyntactic ||
           kind == InterferenceKind::LazySemantic;
}

static bool is_eager(InterferenceKind kind) {
    return !is_lazy(kind);
}

void StrategyFactory::validate(const StrategySpec& spec,
                               const std::string& strategy_name,
                               const std::string& horizon_schedule) {
    // --- Spec-internal constraints ---

    // Lazy interference requires a propagator that uses has_interference()
    // rather than graph methods. NullPropagator and ForallPropagator need
    // get_neighbours() / get_interference_graph() which lazy doesn't support.
    if (is_lazy(spec.interference) && spec.propagator == PropagatorKind::Null) {
        throw std::invalid_argument(
            "Lazy interference requires a propagator (Null propagator needs "
            "graph methods that lazy analysis does not provide)");
    }
    if (is_lazy(spec.interference) && spec.propagator == PropagatorKind::Forall) {
        throw std::invalid_argument(
            "Lazy interference is incompatible with ForallPropagator "
            "(use LazyForall propagator instead)");
    }

    // Eager interference with lazy-only propagators is valid but unusual.
    // No hard constraint — eager provides a superset of lazy's interface.

    // DecisionHeuristicPropagator is designed for exists semantics.
    if (spec.propagator == PropagatorKind::DecisionHeuristic &&
        spec.semantics != SemanticsKind::Exists) {
        throw std::invalid_argument(
            "DecisionHeuristic propagator requires Exists semantics");
    }

    // --- Spec-vs-config constraints ---

    // Non-linear horizon schedules rely on prefix-monotone front-loading,
    // which is only implemented in SequentialPlanner. DoubleTailPlanner
    // tests a specific (forward_end, backward_start) pair per iteration
    // and cannot skip horizons while preserving completeness.
    if (horizon_schedule != "linear" && uses_double_tail(spec)) {
        throw std::invalid_argument(
            "Non-linear horizon schedule '" + horizon_schedule +
            "' is not compatible with double-tail strategy '" + strategy_name + "'. "
            "Use a non-dt strategy or --horizon-schedule linear.");
    }
}

std::unique_ptr<BaseEncoder> StrategyFactory::create_encoder(
    const StrategySpec& spec, const Problem& problem, z3::context& ctx) {
    switch (spec.encoder) {
        case EncoderFamily::Grounded:
            return std::make_unique<GroundedEncoder>(problem, ctx);
        case EncoderFamily::Chained:
            return std::make_unique<ChainedGroundedEncoder>(problem, ctx);
        case EncoderFamily::R2E:
            return std::make_unique<R2EGroundedEncoder>(problem, ctx);
    }
    throw std::invalid_argument("Unknown encoder family");
}

std::unique_ptr<ParallelismStrategy> StrategyFactory::create_parallelism(
    const StrategySpec& spec) {
    switch (spec.semantics) {
        case SemanticsKind::Sequential:
            return std::make_unique<SequentialSemantics>();
        case SemanticsKind::Forall:
            return std::make_unique<ForallSemantics>();
        case SemanticsKind::Exists:
            return std::make_unique<ExistsSemantics>();
    }
    throw std::invalid_argument("Unknown semantics kind");
}

std::unique_ptr<InterferenceAnalysis> StrategyFactory::create_interference(
    const StrategySpec& spec, const Problem& problem) {
    switch (spec.interference) {
        case InterferenceKind::EagerSyntactic:
            return std::make_unique<EagerInterferenceAnalysis>(problem);
        case InterferenceKind::EagerSemantic:
            return std::make_unique<EagerSemanticInterferenceAnalysis>(problem);
        case InterferenceKind::LazySyntactic:
            return std::make_unique<LazyInterferenceAnalysis>(problem);
        case InterferenceKind::LazySemantic:
            return std::make_unique<SemanticInterferenceAnalysis>(problem);
    }
    throw std::invalid_argument("Unknown interference kind");
}

std::unique_ptr<PropagatorStrategy> StrategyFactory::create_propagator(
    const StrategySpec& spec, z3::solver& solver,
    const Problem& problem, const BaseEncoder& encoder) {
    switch (spec.propagator) {
        case PropagatorKind::Null:
            return std::make_unique<NullPropagator>(solver, encoder);
        case PropagatorKind::Forall:
            return std::make_unique<ForallPropagator>(solver, problem, encoder);
        case PropagatorKind::LazyForall:
            return std::make_unique<LazyForallPropagator>(solver, problem, encoder);
        case PropagatorKind::Exists:
            return std::make_unique<ExistsPropagator>(solver, problem, encoder);
        case PropagatorKind::DecisionHeuristic:
            return std::make_unique<DecisionHeuristicPropagator>(solver, problem, encoder);
    }
    throw std::invalid_argument("Unknown propagator kind");
}

std::unique_ptr<BasePlanner> StrategyFactory::create_planner(
    const StrategySpec& spec, const Problem& problem,
    BaseEncoder& encoder, z3::context& ctx) {
    switch (spec.planner) {
        case PlannerKind::Sequential:
            return std::make_unique<SequentialPlanner>(problem, encoder, ctx);
        case PlannerKind::DoubleTail:
            return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }
    throw std::invalid_argument("Unknown planner kind");
}

} // namespace rantanplan
