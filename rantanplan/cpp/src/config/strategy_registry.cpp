#include "strategy_registry.hpp"
#include <stdexcept>
#include <sstream>

namespace rantanplan {

using E = EncoderFamily;
using S = SemanticsKind;
using I = InterferenceKind;
using P = PropagatorKind;
using K = PlannerKind;

const std::map<std::string, StrategyEntry>& StrategyRegistry::get_registry() {
    static const std::map<std::string, StrategyEntry> registry = {
        // Sequential
        {"seq",       {{E::Grounded, S::Sequential, I::None,             P::Null,              K::Sequential},
                       "Sequential encoding (baseline)"}},

        // Forall variants
        {"f",         {{E::Grounded, S::Forall,     I::EagerSyntactic,  P::Null,              K::Sequential},
                       "Forall-step, eager syntactic interference"}},
        {"f-p",       {{E::Grounded, S::Forall,     I::EagerSyntactic,  P::Forall,            K::Sequential},
                       "Forall-step with forall propagator"}},
        {"fl",        {{E::Grounded, S::Forall,     I::LazySyntactic,   P::LazyForall,        K::Sequential},
                       "Forall-step, lazy syntactic interference"}},
        {"fls",       {{E::Grounded, S::Forall,     I::LazySemantic,    P::LazyForall,        K::Sequential},
                       "Forall-step, lazy semantic interference"}},
        {"flsc",      {{E::Chained,  S::Forall,     I::LazySemantic,    P::LazyForall,        K::Sequential},
                       "Forall-step, lazy semantic interference, chained encoder"}},

        // Exists variants
        {"e",         {{E::Grounded, S::Exists,     I::EagerSyntactic,  P::Null,              K::Sequential},
                       "Exists-step, eager syntactic interference"}},
        {"el",        {{E::Grounded, S::Exists,     I::LazySyntactic,   P::Exists,            K::Sequential},
                       "Exists-step, lazy syntactic interference"}},
        {"els",       {{E::Grounded, S::Exists,     I::LazySemantic,    P::Exists,            K::Sequential},
                       "Exists-step, lazy semantic interference"}},
        {"elsc",      {{E::Chained,  S::Exists,     I::LazySemantic,    P::Exists,            K::Sequential},
                       "Exists-step, lazy semantic interference, chained encoder"}},

        // Special strategies
        {"r2e",       {{E::R2E,      S::Sequential, I::None,             P::Null,              K::Sequential},
                       "R2E encoding (relaxed-to-exact)"}},

        // Experimental eager-semantic
        {"fgs",       {{E::Grounded, S::Forall,     I::EagerSemantic,   P::Null,              K::Sequential},
                       "Forall-step, eager semantic interference"}},
        {"fgsc",      {{E::Chained,  S::Forall,     I::EagerSemantic,   P::Null,              K::Sequential},
                       "Forall-step, eager semantic interference, chained encoder"}},
        {"egs",       {{E::Grounded, S::Exists,     I::EagerSemantic,   P::Null,              K::Sequential},
                       "Exists-step, eager semantic interference"}},
        {"egsc",      {{E::Chained,  S::Exists,     I::EagerSemantic,   P::Null,              K::Sequential},
                       "Exists-step, eager semantic interference, chained encoder"}},

        // Double-tail sequential
        {"seq-dt",    {{E::Grounded, S::Sequential, I::None,             P::Null,              K::DoubleTail},
                       "Sequential encoding, double-tail planner"}},

        // Double-tail forall
        {"f-dt",      {{E::Grounded, S::Forall,     I::EagerSyntactic,  P::Null,              K::DoubleTail},
                       "Forall-step, eager syntactic interference, double-tail"}},
        {"fl-dt",     {{E::Grounded, S::Forall,     I::LazySyntactic,   P::LazyForall,        K::DoubleTail},
                       "Forall-step, lazy syntactic interference, double-tail"}},
        {"fls-dt",    {{E::Grounded, S::Forall,     I::LazySemantic,    P::LazyForall,        K::DoubleTail},
                       "Forall-step, lazy semantic interference, double-tail"}},
        {"flsc-dt",   {{E::Chained,  S::Forall,     I::LazySemantic,    P::LazyForall,        K::DoubleTail},
                       "Forall-step, lazy semantic interference, chained encoder, double-tail"}},

        // Double-tail exists
        {"e-dt",      {{E::Grounded, S::Exists,     I::EagerSyntactic,  P::Null,              K::DoubleTail},
                       "Exists-step, eager syntactic interference, double-tail"}},
        {"el-dt",     {{E::Grounded, S::Exists,     I::LazySyntactic,   P::Exists,            K::DoubleTail},
                       "Exists-step, lazy syntactic interference, double-tail"}},
        {"els-dt",    {{E::Grounded, S::Exists,     I::LazySemantic,    P::Exists,            K::DoubleTail},
                       "Exists-step, lazy semantic interference, double-tail"}},
        {"elsc-dt",   {{E::Chained,  S::Exists,     I::LazySemantic,    P::Exists,            K::DoubleTail},
                       "Exists-step, lazy semantic interference, chained encoder, double-tail"}},

        // Frame propagator variant: lazy frame axiom enforcement via user propagator
        {"elsc-fp",   {{E::Chained,  S::Exists,     I::LazySemantic,    P::FrameExists,       K::Sequential},
                       "Exists-step, lazy semantic, chained encoder, frame propagator"}},

        // PDLA: Property-Directed Lazy Activation (see docs/pdla-proposal.md)
        // pdla: always activates blocking literals from cores (fast convergence on dense domains)
        // pdla-sel: selective — blocking literals only as fallback (leaner on sparse domains)
        {"pdla",      {{E::Chained,  S::Exists,     I::LazySemantic,    P::Exists,            K::PDLA},
                       "Property-directed lazy activation"}},
        {"pdla-sel",  {{E::Chained,  S::Exists,     I::LazySemantic,    P::Exists,            K::PDLASelective},
                       "PDLA, selective blocking (leaner on sparse domains)"}},

        // State-aware exists: condition-guarded interference edges via Z3 theory solver.
        // Edges classified as NEVER/ALWAYS/SOMETIMES at activation time.
        // Only SOMETIMES edges get Z3 edge literals; ALWAYS use static check.
        {"pdla-sa",   {{E::Chained,  S::Exists,     I::LazySemantic,    P::StateAwareExists,  K::PDLA},
                       "PDLA with state-aware interference"}},
        {"pdla-sel-sa", {{E::Chained,  S::Exists,     I::LazySemantic,    P::StateAwareExists,  K::PDLASelective},
                       "PDLA selective with state-aware interference"}},
    };
    return registry;
}

const StrategySpec& StrategyRegistry::get(const std::string& name) {
    auto& registry = get_registry();
    auto it = registry.find(name);

    if (it == registry.end()) {
        std::ostringstream error;
        error << "Unknown strategy: '" << name << "'\n";
        error << "Available strategies: ";

        auto strategies = list_strategies();
        for (size_t i = 0; i < strategies.size(); ++i) {
            error << strategies[i];
            if (i < strategies.size() - 1) error << ", ";
        }
        error << "\n";

        throw std::invalid_argument(error.str());
    }

    return it->second.spec;
}

bool StrategyRegistry::exists(const std::string& name) {
    auto& registry = get_registry();
    return registry.find(name) != registry.end();
}

std::vector<std::string> StrategyRegistry::list_strategies() {
    auto& registry = get_registry();
    std::vector<std::string> names;
    names.reserve(registry.size());

    for (const auto& pair : registry) {
        names.push_back(pair.first);
    }

    return names;
}

} // namespace rantanplan
