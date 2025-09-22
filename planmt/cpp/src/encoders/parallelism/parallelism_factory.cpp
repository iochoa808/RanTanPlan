#include "parallelism_factory.hpp"
#include "sequential_semantics.hpp"
#include "forall_semantics.hpp"
#include "exists_semantics.hpp"
#include <algorithm>
#include <cctype>

namespace planmt {

std::unique_ptr<ParallelismStrategy> ParallelismFactory::create_strategy(ParallelismType type) {
    switch (type) {
        case ParallelismType::SEQUENTIAL:
            return std::make_unique<SequentialSemantics>();
        case ParallelismType::FORALL:
            return std::make_unique<ForallSemantics>();
        case ParallelismType::EXISTS:
            return std::make_unique<ExistsSemantics>();
        default:
            return std::make_unique<SequentialSemantics>();
    }
}

std::unique_ptr<ParallelismStrategy> ParallelismFactory::create_strategy(const std::string& strategy_name) {
    ParallelismType type = parse_strategy_type(strategy_name);
    return create_strategy(type);
}

std::string ParallelismFactory::get_strategy_name(ParallelismType type) {
    switch (type) {
        case ParallelismType::SEQUENTIAL:
            return "sequential";
        case ParallelismType::FORALL:
            return "forall";
        case ParallelismType::EXISTS:
            return "exists";
        default:
            return "sequential";
    }
}

ParallelismFactory::ParallelismType ParallelismFactory::parse_strategy_type(const std::string& strategy_name) {
    // Convert to lowercase for case-insensitive comparison
    std::string lower_name = strategy_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower_name == "forall") {
        return ParallelismType::FORALL;
    } else if (lower_name == "exists") {
        return ParallelismType::EXISTS;
    } else {
        // Default to sequential (including "sequential" and any invalid input)
        return ParallelismType::SEQUENTIAL;
    }
}

} // namespace planmt
