#pragma once

#include "strategy_configuration.hpp"
#include "../encoders/grounded_encoder.hpp"
#include "../encoders/chained_grounded_encoder.hpp"
#include "../encoders/r2e_grounded_encoder.hpp"
#include "../planners/double_tail_planner.hpp"
#include "../planners/propagators/null_propagator.hpp"
#include "../planners/propagators/forall_propagator.hpp"
#include "../planners/propagators/lazy_forall_propagator.hpp"
#include "../planners/propagators/exists_propagator.hpp"
#include "../planners/propagators/decision_heuristic_propagator.hpp"
#include "../encoders/parallelism/sequential_semantics.hpp"
#include "../encoders/parallelism/forall_semantics.hpp"
#include "../encoders/parallelism/exists_semantics.hpp"
#include "../encoders/parallelism/eager_interference_analysis.hpp"
#include "../encoders/parallelism/lazy_interference_analysis.hpp"
#include "../encoders/parallelism/semantic_interference_analysis.hpp"
#include "../encoders/parallelism/eager_semantic_interference_analysis.hpp"

namespace rantanplan {

// =============================================================================
// SEQUENTIAL STRATEGIES
// =============================================================================

class SequentialStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<SequentialSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return true; }
    std::string get_name() const override { return "seq"; }
};

// =============================================================================
// FORALL STRATEGIES
// =============================================================================

class ForallStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return true; }
    std::string get_name() const override { return "forall"; }
};

class ForallPropStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<ForallPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "forall-prop"; }
};

class ForallLazyStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<LazyForallPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<LazyInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "forall-lazy"; }
};

class ForallLazySemanticStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<LazyForallPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "forall-lazy-semantic"; }
};

class ForallLazySemanticChainStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<ChainedGroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<LazyForallPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "forall-lazy-semantic-chain"; }
};

// =============================================================================
// EXISTS STRATEGIES
// =============================================================================

class ExistsStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return true; }
    std::string get_name() const override { return "exists"; }
};

class ExistsLazyStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<ExistsPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<LazyInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "exists-lazy"; }
};

class ExistsLazySemanticStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<ExistsPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "exists-lazy-semantic"; }
};

class ExistsLazySemanticChainStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<ChainedGroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<ExistsPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "exists-lazy-semantic-chain"; }
};

// =============================================================================
// SPECIAL STRATEGIES
// =============================================================================

class R2EStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<R2EGroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<SequentialSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return true; }
    std::string get_name() const override { return "r2e"; }
};

class DecisionHeuristicStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<ChainedGroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<DecisionHeuristicPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "dec"; }
};

// =============================================================================
// EXPERIMENTAL EAGER-SEMANTIC STRATEGIES
// =============================================================================

class ForallEagerSemanticStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerSemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return true; }
    std::string get_name() const override { return "forall-eager-semantic"; }
};

class ForallEagerSemanticChainStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<ChainedGroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerSemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return true; }
    std::string get_name() const override { return "forall-eager-semantic-chain"; }
};

class ExistsEagerSemanticStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerSemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return true; }
    std::string get_name() const override { return "exists-eager-semantic"; }
};

class ExistsEagerSemanticChainStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<ChainedGroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerSemanticInterferenceAnalysis>(p);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return true; }
    std::string get_name() const override { return "exists-eager-semantic-chain"; }
};

// =============================================================================
// DOUBLE-TAIL SEQUENTIAL STRATEGIES
// =============================================================================

class SequentialDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<SequentialSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerInterferenceAnalysis>(p);
    }

    // Override to create DoubleTailPlanner instead of SequentialPlanner
    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return true; }
    bool supports_formula_export() const override { return false; }  // Not yet supported
    std::string get_name() const override { return "seq-dt"; }
};

// =============================================================================
// DOUBLE-TAIL FORALL STRATEGIES
// =============================================================================

class ForallDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerInterferenceAnalysis>(p);
    }

    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return true; }
    std::string get_name() const override { return "forall-dt"; }
};

class ForallLazyDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<LazyForallPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<LazyInterferenceAnalysis>(p);
    }

    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "forall-lazy-dt"; }
};

class ForallLazySemanticDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<LazyForallPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "forall-lazy-semantic-dt"; }
};

class ForallLazySemanticChainDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<ChainedGroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ForallSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<LazyForallPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "forall-lazy-semantic-chain-dt"; }
};

// =============================================================================
// DOUBLE-TAIL EXISTS STRATEGIES
// =============================================================================

class ExistsDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<NullPropagator>();
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<EagerInterferenceAnalysis>(p);
    }

    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return true; }
    std::string get_name() const override { return "exists-dt"; }
};

class ExistsLazyDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<ExistsPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<LazyInterferenceAnalysis>(p);
    }

    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "exists-lazy-dt"; }
};

class ExistsLazySemanticDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<GroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<ExistsPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "exists-lazy-semantic-dt"; }
};

class ExistsLazySemanticChainDoubleTailStrategy : public StrategyConfiguration {
public:
    std::unique_ptr<BaseEncoder> create_encoder(const Problem& p, z3::context& ctx) const override {
        return std::make_unique<ChainedGroundedEncoder>(p, ctx);
    }

    std::unique_ptr<ParallelismStrategy> create_parallelism() const override {
        return std::make_unique<ExistsSemantics>();
    }

    std::unique_ptr<PropagatorStrategy> create_propagator(
        z3::solver& s, const Problem& p, const BaseEncoder& e) const override {
        return std::make_unique<ExistsPropagator>(s, p, e);
    }

    std::unique_ptr<InterferenceAnalysis> create_interference(const Problem& p) const override {
        return std::make_unique<SemanticInterferenceAnalysis>(p);
    }

    std::unique_ptr<BasePlanner> create_planner(
        const Problem& problem, BaseEncoder& encoder, z3::context& ctx) const override {
        return std::make_unique<DoubleTailPlanner>(problem, encoder, ctx);
    }

    bool needs_parallelism_encoding() const override { return false; }
    std::string get_name() const override { return "exists-lazy-semantic-chain-dt"; }
};

} // namespace rantanplan
