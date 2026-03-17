#pragma once

#include "arithmetic_profile.hpp"
#include "../problem/problem.hpp"

namespace rantanplan {

/**
 * @brief Result of numeric constraint analysis.
 */
struct ConstraintAnalysisResult {
    ArithmeticProfile profile = ArithmeticProfile::NONE;
    size_t num_numeric_fluents = 0;
    bool all_integer = false;  ///< True iff all numeric fluents are int-typed,
                               ///< no variable division, all constants integer-valued
};

/**
 * @brief Analyzes the numeric constraint structure of a grounded planning problem.
 *
 * Walks all preconditions, goals, effects, and costs in the ExprPool to classify
 * the problem's arithmetic complexity. The result is used to select the optimal
 * Z3 SMT logic string for solver tuning.
 */
class NumericConstraintAnalyzer {
public:
    static ConstraintAnalysisResult analyze(const Problem& problem);

private:
    struct TermDescriptor {
        int var_count = 0;
        bool is_linear = true;
    };

    static ArithmeticProfile classify_formula(ExprID id, const ExprPool& pool, const Problem& problem);
    static TermDescriptor classify_arithmetic_expr(ExprID id, const ExprPool& pool, const Problem& problem);
    static ArithmeticProfile comparison_profile(const TermDescriptor& lhs, const TermDescriptor& rhs);
};

} // namespace rantanplan
