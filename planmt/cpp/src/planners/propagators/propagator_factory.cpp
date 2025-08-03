#include "propagator_factory.h"
#include "null_propagator.h"
#include "forall_propagator.h"
#include "lazy_forall_propagator.h"
#include "exists_propagator.h"
#include <algorithm>
#include <cctype>

namespace planmt {

std::unique_ptr<PropagatorStrategy> PropagatorFactory::create_strategy(
    PropagatorType type, 
    z3::solver& solver,
    const Problem& problem) {
    
    switch (type) {
        case PropagatorType::NULL_PROPAGATOR:
            return std::make_unique<NullPropagator>();
        case PropagatorType::FORALL:
            return std::make_unique<ForallPropagator>(solver, problem);
        case PropagatorType::LAZY_FORALL:
            return std::make_unique<LazyForallPropagator>(solver, problem);
        case PropagatorType::EXISTS:
            return std::make_unique<ExistsPropagator>(solver, problem);
        default:
            return std::make_unique<NullPropagator>();
    }
}

std::unique_ptr<PropagatorStrategy> PropagatorFactory::create_strategy(
    const std::string& strategy_name,
    z3::solver& solver,
    const Problem& problem) {
    
    PropagatorType type = parse_strategy_type(strategy_name);
    return create_strategy(type, solver, problem);
}

std::string PropagatorFactory::get_strategy_name(PropagatorType type) {
    switch (type) {
        case PropagatorType::NULL_PROPAGATOR:
            return "null";
        case PropagatorType::FORALL:
            return "forall";
        case PropagatorType::LAZY_FORALL:
            return "lazy_forall";
        case PropagatorType::EXISTS:
            return "exists";
        default:
            return "null";
    }
}

PropagatorType PropagatorFactory::parse_strategy_type(const std::string& strategy_name) {
    // Convert to lowercase for case-insensitive comparison
    std::string lower_name = strategy_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower_name == "forall") {
        return PropagatorType::FORALL;
    } else if (lower_name == "lazy_forall") {
        return PropagatorType::LAZY_FORALL;
    } else if (lower_name == "exists") {
        return PropagatorType::EXISTS;
    } else if (lower_name == "null" || lower_name == "none") {
        return PropagatorType::NULL_PROPAGATOR;
    } else {
        // Default to null propagator (including any invalid input)
        return PropagatorType::NULL_PROPAGATOR;
    }
}

std::vector<std::string> PropagatorFactory::get_available_types() {
    return {"null", "forall", "lazy_forall", "exists"};
}

} // namespace planmt
