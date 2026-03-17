#pragma once

#include <string>

namespace rantanplan {

/**
 * @brief Classifies the numeric constraint structure of a planning problem.
 *
 * Used to select the optimal Z3 SMT logic string, which triggers
 * comprehensive parameter tuning (relevancy, phase selection, restarts,
 * arithmetic solver backend, etc.).
 */
enum class ArithmeticProfile {
    NONE = 0,              // Purely propositional (no numeric fluents)
    DIFFERENCE_LOGIC = 1,  // x - y <= c, x <= c (at most 2 vars per comparison, unit coefficients)
    LINEAR = 2,            // General linear arithmetic (a1*x1 + a2*x2 + ... <= c)
    NONLINEAR = 3          // Variable*variable, variable/variable, abs/mod/min/max with variables
};

inline std::string arithmetic_profile_to_string(ArithmeticProfile p) {
    switch (p) {
        case ArithmeticProfile::NONE:             return "none";
        case ArithmeticProfile::DIFFERENCE_LOGIC: return "difference_logic";
        case ArithmeticProfile::LINEAR:           return "linear";
        case ArithmeticProfile::NONLINEAR:        return "nonlinear";
        default:                                  return "unknown";
    }
}

/**
 * @brief Returns the recommended Z3 logic string for the given profile.
 *
 * Logic strings trigger comprehensive SMT parameter tuning in Z3's
 * smt_setup.cpp (relevancy, phase selection, restarts, etc.).
 * All logics use solver 6 (modern LRA) — the gains come from SMT-level
 * parameter tuning, not from switching arithmetic backends.
 *
 * When all_integer is true (all numeric fluents are int-typed, no division,
 * all constants integer-valued), integer logics (QF_IDL, QF_LIA) are used.
 * These enable tighter integer-specific reasoning in Z3's theory solver.
 *
 * For nonlinear problems, we still use QF_LRA/QF_LIA — solver 6's embedded
 * nla::solver handles nonlinear constraints automatically. QF_NRA would
 * trigger NLSAT which is incompatible with incremental solving.
 *
 * @return Logic string, or nullptr if no logic hint is appropriate.
 */
inline const char* recommended_logic(ArithmeticProfile p, bool all_integer = false) {
    switch (p) {
        case ArithmeticProfile::NONE:             return nullptr;
        case ArithmeticProfile::DIFFERENCE_LOGIC: return all_integer ? "QF_IDL" : "QF_RDL";
        case ArithmeticProfile::LINEAR:           return all_integer ? "QF_LIA" : "QF_LRA";
        case ArithmeticProfile::NONLINEAR:        return all_integer ? "QF_LIA" : "QF_LRA";
        default:                                  return nullptr;
    }
}

} // namespace rantanplan
