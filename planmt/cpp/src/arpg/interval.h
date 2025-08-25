#pragma once

#include <iostream>
#include <limits>
#include <cmath>

namespace planmt {

/**
 * @brief Interval class for interval arithmetic operations
 * 
 * Represents closed intervals [lower, upper] with support for arithmetic operations.
 * Based on the interval operations described in the ARPG paper (Section 3.1.1).
 */
class Interval {
public:
    // Constructors
    Interval() : lower_(-std::numeric_limits<double>::infinity()), 
                 upper_(std::numeric_limits<double>::infinity()) {}
    
    Interval(double value) : lower_(value), upper_(value) {}
    
    Interval(double lower, double upper) : lower_(lower), upper_(upper) {
        if (lower > upper) {
            std::swap(lower_, upper_);
        }
    }
    
    // Accessors
    double lower() const { return lower_; }
    double upper() const { return upper_; }
    bool is_empty() const { return lower_ > upper_; }
    bool is_unbounded() const { 
        return std::isinf(lower_) || std::isinf(upper_); 
    }
    
    // Get the midpoint of the interval
    double midpoint() const {
        if (is_unbounded()) {
            // For infinite intervals, return a reasonable default
            if (std::isinf(lower_) && std::isinf(upper_)) {
                return 0.0;  // (-∞, +∞) -> 0
            } else if (std::isinf(lower_) && lower_ < 0) {
                return upper_ - 1.0;  // (-∞, x) -> x-1
            } else if (std::isinf(upper_) && upper_ > 0) {
                return lower_ + 1.0;  // (x, +∞) -> x+1
            }
        }
        return (lower_ + upper_) / 2.0;
    }
    
    // Containment checks
    bool contains(double value) const {
        return value >= lower_ && value <= upper_;
    }
    
    bool contains(const Interval& other) const {
        return lower_ <= other.lower_ && upper_ >= other.upper_;
    }
    
    // Interval arithmetic operations (as per paper Section 3.1.1)
    Interval operator+(const Interval& other) const {
        return Interval(lower_ + other.lower_, upper_ + other.upper_);
    }
    
    Interval operator-(const Interval& other) const {
        return Interval(lower_ - other.upper_, upper_ - other.lower_);
    }
    
    Interval operator*(const Interval& other) const {
        double ll = lower_ * other.lower_;
        double lu = lower_ * other.upper_;
        double ul = upper_ * other.lower_;
        double uu = upper_ * other.upper_;
        
        return Interval(std::min({ll, lu, ul, uu}), std::max({ll, lu, ul, uu}));
    }
    
    // Convex union operation (Definition 4 in paper)
    Interval convex_union(const Interval& other) const {
        return Interval(std::min(lower_, other.lower_), std::max(upper_, other.upper_));
    }
    
    // String representation
    std::string to_string() const {
        if (is_empty()) {
            return "[]";
        }
        return "[" + std::to_string(lower_) + ", " + std::to_string(upper_) + "]";
    }
    
    // Comparison operators
    bool operator==(const Interval& other) const {
        return lower_ == other.lower_ && upper_ == other.upper_;
    }
    
    bool operator!=(const Interval& other) const {
        return !(*this == other);
    }

private:
    double lower_;
    double upper_;
};

} // namespace planmt