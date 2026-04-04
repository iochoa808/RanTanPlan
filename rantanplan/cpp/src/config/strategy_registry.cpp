#include "strategy_registry.hpp"
#include <stdexcept>
#include <sstream>

namespace rantanplan {

using E = EncoderFamily;
using S = SemanticsKind;
using I = InterferenceKind;
using P = PropagatorKind;
using K = PlannerKind;

const std::map<std::string, StrategySpec>& StrategyRegistry::get_registry() {
    static const std::map<std::string, StrategySpec> registry = {
        // Sequential
        {"seq",                            {E::Grounded, S::Sequential, I::None,             P::Null,              K::Sequential}},

        // Forall variants
        {"forall",                         {E::Grounded, S::Forall,     I::EagerSyntactic,  P::Null,              K::Sequential}},
        {"forall-prop",                    {E::Grounded, S::Forall,     I::EagerSyntactic,  P::Forall,            K::Sequential}},
        {"forall-lazy",                    {E::Grounded, S::Forall,     I::LazySyntactic,   P::LazyForall,        K::Sequential}},
        {"forall-lazy-semantic",           {E::Grounded, S::Forall,     I::LazySemantic,    P::LazyForall,        K::Sequential}},
        {"forall-lazy-semantic-chain",     {E::Chained,  S::Forall,     I::LazySemantic,    P::LazyForall,        K::Sequential}},

        // Exists variants
        {"exists",                         {E::Grounded, S::Exists,     I::EagerSyntactic,  P::Null,              K::Sequential}},
        {"exists-lazy",                    {E::Grounded, S::Exists,     I::LazySyntactic,   P::Exists,            K::Sequential}},
        {"exists-lazy-semantic",           {E::Grounded, S::Exists,     I::LazySemantic,    P::Exists,            K::Sequential}},
        {"exists-lazy-semantic-chain",     {E::Chained,  S::Exists,     I::LazySemantic,    P::Exists,            K::Sequential}},

        // Special strategies
        {"r2e",                            {E::R2E,      S::Sequential, I::None,             P::Null,              K::Sequential}},

        // Experimental eager-semantic
        {"forall-eager-semantic",          {E::Grounded, S::Forall,     I::EagerSemantic,   P::Null,              K::Sequential}},
        {"forall-eager-semantic-chain",    {E::Chained,  S::Forall,     I::EagerSemantic,   P::Null,              K::Sequential}},
        {"exists-eager-semantic",          {E::Grounded, S::Exists,     I::EagerSemantic,   P::Null,              K::Sequential}},
        {"exists-eager-semantic-chain",    {E::Chained,  S::Exists,     I::EagerSemantic,   P::Null,              K::Sequential}},

        // Double-tail sequential
        {"seq-dt",                         {E::Grounded, S::Sequential, I::None,             P::Null,              K::DoubleTail}},

        // Double-tail forall
        {"forall-dt",                      {E::Grounded, S::Forall,     I::EagerSyntactic,  P::Null,              K::DoubleTail}},
        {"forall-lazy-dt",                 {E::Grounded, S::Forall,     I::LazySyntactic,   P::LazyForall,        K::DoubleTail}},
        {"forall-lazy-semantic-dt",        {E::Grounded, S::Forall,     I::LazySemantic,    P::LazyForall,        K::DoubleTail}},
        {"forall-lazy-semantic-chain-dt",  {E::Chained,  S::Forall,     I::LazySemantic,    P::LazyForall,        K::DoubleTail}},

        // Double-tail exists
        {"exists-dt",                      {E::Grounded, S::Exists,     I::EagerSyntactic,  P::Null,              K::DoubleTail}},
        {"exists-lazy-dt",                 {E::Grounded, S::Exists,     I::LazySyntactic,   P::Exists,            K::DoubleTail}},
        {"exists-lazy-semantic-dt",        {E::Grounded, S::Exists,     I::LazySemantic,    P::Exists,            K::DoubleTail}},
        {"exists-lazy-semantic-chain-dt",  {E::Chained,  S::Exists,     I::LazySemantic,    P::Exists,            K::DoubleTail}},

        // Lazy R2E with flowing frontier (core-guided, no interference/propagators needed)
        {"lazy-r2e",                       {E::Grounded, S::Sequential, I::None,             P::Null,              K::LazyR2E}},
        {"causal-lazy-r2e",                {E::Grounded, S::Sequential, I::None,             P::Null,              K::CausalLazyR2E}},

        // Causal exists: core-guided lazy activation over exists-step encoding
        {"causal-exists",                  {E::Chained,  S::Exists,     I::LazySemantic,    P::Exists,            K::CausalExists}},

        // Frame propagator variants: lazy frame axiom enforcement via user propagator
        {"exists-lazy-semantic-chain-fp",  {E::Chained,  S::Exists,     I::LazySemantic,    P::FrameExists,       K::Sequential}},
        {"causal-exists-fp",               {E::Chained,  S::Exists,     I::LazySemantic,    P::FrameExists,       K::CausalExists}},

        // Guided causal-exists: condition-based substitution + tracked preconditions
        {"causal-exists-guided",           {E::Chained,  S::Exists,     I::LazySemantic,    P::Exists,            K::CausalExistsGuided}},
        {"causal-exists-guided-fp",        {E::Chained,  S::Exists,     I::LazySemantic,    P::FrameExists,       K::CausalExistsGuided}},

        // PDLA: Property-Directed Lazy Activation (see docs/pdla-proposal.md)
        {"pdla",                           {E::Chained,  S::Exists,     I::LazySemantic,    P::FrameExists,       K::PDLA}},
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

    return it->second;
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
