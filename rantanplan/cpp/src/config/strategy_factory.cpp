#include "strategy_factory.hpp"

#include "../encoders/grounded_encoder.hpp"
#include "../encoders/chained_grounded_encoder.hpp"
#include "../encoders/r2e_grounded_encoder.hpp"

#include "../encoders/parallelism/sequential_semantics.hpp"
#include "../encoders/parallelism/forall_semantics.hpp"
#include "../encoders/parallelism/exists_semantics.hpp"

#include "../analysis/eager_interference_analysis.hpp"
#include "../analysis/eager_semantic_interference_analysis.hpp"
#include "../analysis/lazy_interference_analysis.hpp"
#include "../analysis/semantic_interference_analysis.hpp"

#include "../planners/sequential.hpp"
#include "../planners/double_tail_planner.hpp"
#include "../planners/branch_and_bound_planner.hpp"

#include "../planners/propagators/null_propagator.hpp"
#include "../planners/propagators/forall_propagator.hpp"
#include "../planners/propagators/lazy_forall_propagator.hpp"
#include "../planners/propagators/exists_propagator.hpp"
#include "../planners/propagators/decision_heuristic_propagator.hpp"

#include "../util/logger.hpp"

#include <algorithm>
#include <stdexcept>

namespace rantanplan {

static bool is_lazy(InterferenceKind kind) {
    return kind == InterferenceKind::LazySyntactic ||
           kind == InterferenceKind::LazySemantic;
}

static bool is_none(InterferenceKind kind) {
    return kind == InterferenceKind::None;
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

    // No interference is only valid with sequential semantics (which has
    // built-in at-most-one constraint and doesn't need interference analysis).
    if (is_none(spec.interference) && spec.semantics != SemanticsKind::Sequential) {
        throw std::invalid_argument(
            "No interference analysis requires Sequential semantics "
            "(Forall/Exists semantics need interference for mutex constraints)");
    }

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

void StrategyFactory::adjust_spec(StrategySpec& spec, const Problem& problem) {
    // SDAC + exists-step is unsound for cost-optimal planning: exists semantics
    // evaluates costs at x_t but the serialized cost depends on intermediate
    // states. Downgrade to forall semantics which is SDAC-safe.
    if (uses_branch_and_bound(spec) &&
        problem.has_metric() && problem.has_state_dependent_costs() &&
        sdac_unsafe(spec)) {
        Logger::instance().info(
            "SDAC detected with exists-step semantics — downgrading to "
            "forall semantics for sound cost evaluation.");
        spec.semantics = SemanticsKind::Forall;
        if (spec.propagator == PropagatorKind::Exists) {
            spec.propagator = PropagatorKind::LazyForall;
        }
    }

    if (uses_branch_and_bound(spec) &&
        problem.has_metric() && problem.has_state_dependent_costs()) {
        Logger::instance().info(
            "SDAC detected: abstract suffix uses RPG-derived cost lower bounds.");
    }
}

void StrategyFactory::configure_planner(BasePlanner& planner, const StrategySpec& spec,
                                         const PipelineResult& pipeline_result) {
    if (!uses_branch_and_bound(spec) || pipeline_result.sdac_cost_lower_bounds.empty()) {
        return;
    }

    const auto& cost_bounds = pipeline_result.sdac_cost_lower_bounds;
    bool all_positive = std::all_of(cost_bounds.begin(), cost_bounds.end(),
                                     [](double lb) { return lb > 0.0; });
    if (!all_positive) {
        Logger::instance().info(
            "WARNING: Some SDAC action cost expressions have a lower bound "
            "of 0 (or could not be bounded). The abstract suffix will use 0 "
            "for these actions, reducing branch-and-bound pruning power.");
    } else {
        Logger::instance().info(
            "SDAC cost lower bounds computed via numeric RPG fixpoint — "
            "all action costs have positive lower bounds.");
    }

    auto* bb_planner = dynamic_cast<BranchAndBoundPlanner*>(&planner);
    if (bb_planner) {
        bb_planner->set_cost_lower_bounds(
            std::vector<double>(cost_bounds.begin(), cost_bounds.end()));
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
        case InterferenceKind::None:
            return nullptr;
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
    BaseEncoder& encoder, z3::context& ctx,
    const InterferenceAnalysis* interference) {
    switch (spec.planner) {
        case PlannerKind::Sequential:
            return std::make_unique<SequentialPlanner>(problem, encoder, ctx);
        case PlannerKind::DoubleTail:
            return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
        case PlannerKind::BranchAndBound:
            return std::make_unique<BranchAndBoundPlanner>(
                problem, encoder, ctx, interference, spec.semantics);
    }
    throw std::invalid_argument("Unknown planner kind");
}

} // namespace rantanplan
