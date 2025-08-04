#include "interference_analysis_factory.h"
#include "eager_interference_analysis.h"
#include "lazy_interference_analysis.h"
#include "../../config/config.h"
#include <iostream>

namespace planmt {

std::unique_ptr<InterferenceAnalysis> InterferenceAnalysisFactory::create(const Problem& problem, bool use_lazy) {
    if (use_lazy) {
        std::cout << "Creating LazyInterferenceAnalysis" << std::endl;
        return std::make_unique<LazyInterferenceAnalysis>(problem);
    } else {
        std::cout << "Creating EagerInterferenceAnalysis" << std::endl;
        return std::make_unique<EagerInterferenceAnalysis>(problem);
    }
}

std::unique_ptr<InterferenceAnalysis> InterferenceAnalysisFactory::create_from_config(const Problem& problem) {
    const Config& config = Config::instance();
    bool use_lazy = config.interference_analyzer.lazy_computation;
    return create(problem, use_lazy);
}

} // namespace planmt