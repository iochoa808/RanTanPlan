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
#include "../planners/pdla_planner.hpp"

#include "../planners/propagators/modules/exists_cycle_module.hpp"
#include "../planners/propagators/modules/forall_mutex_module.hpp"
#include "../planners/propagators/modules/frame_axiom_module.hpp"
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
    // Lazy interference requires a propagator that uses has_interference()
    // rather than graph methods. ForallEager needs get_neighbours().
    if (is_lazy(spec.interference) && spec.propagator.empty()) {
        throw std::invalid_argument(
            "Lazy interference requires a propagator (empty propagator needs "
            "graph methods that lazy analysis does not provide)");
    }
    if (is_lazy(spec.interference) &&
        spec.propagator.count(PropagatorModuleKind::ForallEager)) {
        throw std::invalid_argument(
            "Lazy interference is incompatible with ForallEager "
            "(use ForallLazy instead)");
    }

    if (is_none(spec.interference) && spec.semantics != SemanticsKind::Sequential) {
        throw std::invalid_argument(
            "No interference analysis requires Sequential semantics "
            "(Forall/Exists semantics need interference for mutex constraints)");
    }

    if (horizon_schedule != "linear" && uses_double_tail(spec)) {
        throw std::invalid_argument(
            "Non-linear horizon schedule '" + horizon_schedule +
            "' is not compatible with double-tail strategy '" + strategy_name + "'. "
            "Use a non-dt strategy or --horizon-schedule linear.");
    }
}

void StrategyFactory::adjust_spec(StrategySpec& spec, const Problem& problem) {
    if (uses_branch_and_bound(spec) &&
        problem.has_metric() && problem.has_state_dependent_costs() &&
        sdac_unsafe(spec)) {
        Logger::instance().info(
            "SDAC detected with exists-step semantics — downgrading to "
            "forall semantics for sound cost evaluation.");
        spec.semantics = SemanticsKind::Forall;
        if (spec.propagator.count(PropagatorModuleKind::ExistsCycle)) {
            spec.propagator.erase(PropagatorModuleKind::ExistsCycle);
            spec.propagator.insert(PropagatorModuleKind::ForallLazy);
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
    const Problem& problem, BaseEncoder& encoder) {

    const auto& mods = spec.propagator;

    // Enable lazy frame mode in the encoder if frame axioms are active
    if (mods.count(PropagatorModuleKind::FrameAxiom)) {
        auto* grounded = dynamic_cast<GroundedEncoder*>(&encoder);
        if (grounded) grounded->set_lazy_frames(true);
    }

    auto composite = std::make_unique<PropagatorStrategy>(solver, problem, encoder);

    // Parallelism modules (order: first, cheapest conflict source).
    if (mods.count(PropagatorModuleKind::ForallEager))
        composite->add_module(std::make_unique<ForallMutexModule>());
    if (mods.count(PropagatorModuleKind::ForallLazy))
        composite->add_module(std::make_unique<LazyForallMutexModule>());
    if (mods.count(PropagatorModuleKind::ExistsCycle))
        composite->add_module(std::make_unique<ExistsCycleModule>());

    // Frame axiom module (after parallelism — more expensive, skipped on conflict)
    if (mods.count(PropagatorModuleKind::FrameAxiom))
        composite->add_module(std::make_unique<FrameAxiomModule>());

    return composite;
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
        case PlannerKind::PDLA:
            return std::make_unique<PDLAPlanner>(problem, encoder, ctx);
        case PlannerKind::PDLASelective: {
            auto p = std::make_unique<PDLAPlanner>(problem, encoder, ctx);
            p->set_always_activate_core(false);
            return p;
        }
        case PlannerKind::PDLAFinegrained: {
            auto p = std::make_unique<PDLAPlanner>(problem, encoder, ctx);
            p->set_finegrained(true);
            return p;
        }
    }
    throw std::invalid_argument("Unknown planner kind");
}

} // namespace rantanplan
