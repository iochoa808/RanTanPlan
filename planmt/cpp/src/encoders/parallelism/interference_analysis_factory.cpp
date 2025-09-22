#include "interference_analysis_factory.hpp"
#include "eager_interference_analysis.hpp"
#include "lazy_interference_analysis.hpp"
#include "semantic_interference_analysis.hpp"
#include "../../config/config.hpp"
#include <iostream>
#include <stdexcept>

namespace planmt {

std::unique_ptr<InterferenceAnalysis> InterferenceAnalysisFactory::create(const Problem& problem, const std::string& analysis_type) {
    if (analysis_type == "semantic") {
        std::cout << "Creating SemanticInterferenceAnalysis (inherently lazy)" << std::endl;
        return std::make_unique<SemanticInterferenceAnalysis>(problem);
    } else if (analysis_type == "lazy") {
        std::cout << "Creating LazyInterferenceAnalysis (syntactic)" << std::endl;
        return std::make_unique<LazyInterferenceAnalysis>(problem);
    } else if (analysis_type == "eager") {
        std::cout << "Creating EagerInterferenceAnalysis (syntactic)" << std::endl;
        return std::make_unique<EagerInterferenceAnalysis>(problem);
    } else {
        throw std::invalid_argument("Invalid analysis_type '" + analysis_type + "'. Valid options: 'eager', 'lazy', 'semantic'");
    }
}

std::unique_ptr<InterferenceAnalysis> InterferenceAnalysisFactory::create_from_config(const Problem& problem) {
    const Config& config = Config::instance();
    return create(problem, config.interference_analyzer.type);
}

} // namespace planmt