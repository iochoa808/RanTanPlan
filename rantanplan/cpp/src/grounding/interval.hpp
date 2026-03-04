#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace rantanplan {

/// Lightweight closed interval [lower, upper] for numeric bounds in grounding.
/// Self-contained — no dependency on the ARPG Interval class.
struct Interval {
    double lower;
    double upper;

    /// Point interval [val, val].
    explicit Interval(double val) : lower(val), upper(val) {}

    /// Range interval [lo, hi], auto-swaps if reversed.
    Interval(double lo, double hi)
        : lower(std::min(lo, hi)), upper(std::max(lo, hi)) {}

    /// Full range (unknown / uninitialized).
    static Interval unbounded() {
        return {-std::numeric_limits<double>::infinity(),
                 std::numeric_limits<double>::infinity()};
    }

    // -- Interval arithmetic --------------------------------------------------

    Interval operator+(const Interval& o) const {
        return {lower + o.lower, upper + o.upper};
    }

    Interval operator-(const Interval& o) const {
        return {lower - o.upper, upper - o.lower};
    }

    Interval operator*(const Interval& o) const {
        double a = lower * o.lower, b = lower * o.upper;
        double c = upper * o.lower, d = upper * o.upper;
        return {std::min({a, b, c, d}), std::max({a, b, c, d})};
    }

    Interval operator/(const Interval& o) const {
        // Division by interval crossing zero → unbounded (safe over-approx).
        if (o.lower <= 0.0 && o.upper >= 0.0) return unbounded();
        double a = lower / o.lower, b = lower / o.upper;
        double c = upper / o.lower, d = upper / o.upper;
        return {std::min({a, b, c, d}), std::max({a, b, c, d})};
    }

    // -- Set operations -------------------------------------------------------

    /// Convex union: [min(lowers), max(uppers)].
    Interval convex_union(const Interval& o) const {
        return {std::min(lower, o.lower), std::max(upper, o.upper)};
    }

    /// True if the intervals overlap (share at least one point).
    bool overlaps(const Interval& o) const {
        return lower <= o.upper && o.lower <= upper;
    }

    bool is_unbounded() const {
        return std::isinf(lower) || std::isinf(upper);
    }

    bool operator==(const Interval& o) const {
        return lower == o.lower && upper == o.upper;
    }

    bool operator!=(const Interval& o) const { return !(*this == o); }

    std::string to_string() const {
        return "[" + std::to_string(lower) + ", " + std::to_string(upper) + "]";
    }
};

} // namespace rantanplan
