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
 * When has_arrays is true (the problem contains array or set fluents encoded
 * natively as Z3 Array sort variables), the array theory fragment is added.
 * The SMT-LIB2 convention is the prefix "A" before the arithmetic fragment:
 *   QF_LIA  → QF_ALIA,  QF_LRA → QF_ALRA
 *   no arithmetic → QF_AX  (pure array extensionality)
 * Difference logic + arrays also maps to QF_ALIA/QF_ALRA: "QF_AIDL"/"QF_ARDL"
 * are not real SMT-LIB logics, and Z3 does not error on unknown strings — it
 * silently selects the GENERIC solver, wasting exactly the tuned-tactic
 * benefit the hint exists for.
 *
 * When uf_arrays is true (--array-encoding uf), array/set reads are
 * uninterpreted function applications (plus Theory-array leaves for
 * array-of-sets), so the logic must include UF too: QF_AUFLIA — which has a
 * dedicated Z3 tactic — for the all-integer case. Z3 ships no QF_AUFLRA
 * tactic, so the mixed-real UF case returns nullptr (default solver) instead
 * of a fake string.
 *
 * @return Logic string, or nullptr if no logic hint is appropriate.
 */
// [XTS] all_integer/has_arrays/uf_arrays parameters added; array logic selection is XTS.
inline const char* recommended_logic(ArithmeticProfile p, bool all_integer = false,
                                     bool has_arrays = false, bool uf_arrays = false) {
    if (has_arrays && uf_arrays) {
        return all_integer ? "QF_AUFLIA" : nullptr;  // [XTS-UnFun]
    }
    switch (p) {
        case ArithmeticProfile::NONE:
            return has_arrays ? "QF_AX" : nullptr;
        case ArithmeticProfile::DIFFERENCE_LOGIC:
            return all_integer ? (has_arrays ? "QF_ALIA" : "QF_IDL")   // [XTS]
                               : (has_arrays ? "QF_ALRA" : "QF_RDL");  // [XTS]
        case ArithmeticProfile::LINEAR:
            return all_integer ? (has_arrays ? "QF_ALIA" : "QF_LIA")   // [XTS]
                               : (has_arrays ? "QF_ALRA" : "QF_LRA");  // [XTS]
        case ArithmeticProfile::NONLINEAR:
            return all_integer ? (has_arrays ? "QF_ALIA" : "QF_LIA")   // [XTS]
                               : (has_arrays ? "QF_ALRA" : "QF_LRA");  // [XTS]
        default:
            return nullptr;
    }
}

} // namespace rantanplan
