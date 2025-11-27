#include "strategy_registry.hpp"
#include "strategies.hpp"
#include <stdexcept>
#include <sstream>

namespace rantanplan {

std::map<std::string, StrategyRegistry::StrategyFactory>& StrategyRegistry::get_registry() {
    static std::map<std::string, StrategyFactory> registry;
    static bool initialized = false;

    if (!initialized) {
        initialized = true;  // Set BEFORE calling initialize to avoid recursion
        initialize_builtin_strategies(registry);
    }

    return registry;
}

void StrategyRegistry::initialize_builtin_strategies(std::map<std::string, StrategyFactory>& registry) {

    // Sequential
    registry["seq"] = []() { return std::make_unique<SequentialStrategy>(); };

    // Forall variants
    registry["forall"] = []() { return std::make_unique<ForallStrategy>(); };
    registry["forall-prop"] = []() { return std::make_unique<ForallPropStrategy>(); };
    registry["forall-lazy"] = []() { return std::make_unique<ForallLazyStrategy>(); };
    registry["forall-lazy-semantic"] = []() { return std::make_unique<ForallLazySemanticStrategy>(); };
    registry["forall-lazy-semantic-chain"] = []() { return std::make_unique<ForallLazySemanticChainStrategy>(); };

    // Exists variants
    registry["exists"] = []() { return std::make_unique<ExistsStrategy>(); };
    registry["exists-lazy"] = []() { return std::make_unique<ExistsLazyStrategy>(); };
    registry["exists-lazy-semantic"] = []() { return std::make_unique<ExistsLazySemanticStrategy>(); };
    registry["exists-lazy-semantic-chain"] = []() { return std::make_unique<ExistsLazySemanticChainStrategy>(); };

    // Special strategies
    registry["r2e"] = []() { return std::make_unique<R2EStrategy>(); };
    registry["dec"] = []() { return std::make_unique<DecisionHeuristicStrategy>(); };

    // Experimental
    registry["forall-eager-semantic"] = []() { return std::make_unique<ForallEagerSemanticStrategy>(); };
    registry["forall-eager-semantic-chain"] = []() { return std::make_unique<ForallEagerSemanticChainStrategy>(); };
    registry["exists-eager-semantic"] = []() { return std::make_unique<ExistsEagerSemanticStrategy>(); };
    registry["exists-eager-semantic-chain"] = []() { return std::make_unique<ExistsEagerSemanticChainStrategy>(); };

    // Double-tail strategies
    registry["seq-dt"] = []() { return std::make_unique<SequentialDoubleTailStrategy>(); };
    registry["forall-dt"] = []() { return std::make_unique<ForallDoubleTailStrategy>(); };
    registry["forall-lazy-dt"] = []() { return std::make_unique<ForallLazyDoubleTailStrategy>(); };
    registry["forall-lazy-semantic-dt"] = []() { return std::make_unique<ForallLazySemanticDoubleTailStrategy>(); };
    registry["forall-lazy-semantic-chain-dt"] = []() { return std::make_unique<ForallLazySemanticChainDoubleTailStrategy>(); };
    registry["exists-dt"] = []() { return std::make_unique<ExistsDoubleTailStrategy>(); };
    registry["exists-lazy-dt"] = []() { return std::make_unique<ExistsLazyDoubleTailStrategy>(); };
    registry["exists-lazy-semantic-dt"] = []() { return std::make_unique<ExistsLazySemanticDoubleTailStrategy>(); };
    registry["exists-lazy-semantic-chain-dt"] = []() { return std::make_unique<ExistsLazySemanticChainDoubleTailStrategy>(); };
}

std::unique_ptr<StrategyConfiguration> StrategyRegistry::create(const std::string& name) {
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

    return it->second();
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
