#include "real.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace planmt {

Real::Real(double value) {
    // Convert double to fraction (simple approach)
    const int64_t precision = 1000000; // 6 decimal places
    numerator_ = static_cast<int64_t>(value * precision);
    denominator_ = precision;
    normalize();
}

Real::Real(const pb::Real& pb_real) 
    : numerator_(pb_real.numerator()), denominator_(pb_real.denominator()) {
    normalize();
}

double Real::to_double() const {
    return static_cast<double>(numerator_) / static_cast<double>(denominator_);
}

std::string Real::to_string() const {
    if (denominator_ == 1) {
        return std::to_string(numerator_);
    }
    return std::to_string(numerator_) + "/" + std::to_string(denominator_);
}

pb::Real Real::to_protobuf() const {
    pb::Real pb_real;
    pb_real.set_numerator(numerator_);
    pb_real.set_denominator(denominator_);
    return pb_real;
}

bool Real::operator==(const Real& other) const {
    // Compare cross products to avoid floating point issues
    return numerator_ * other.denominator_ == other.numerator_ * denominator_;
}

bool Real::operator<(const Real& other) const {
    // Compare cross products
    return numerator_ * other.denominator_ < other.numerator_ * denominator_;
}

bool Real::operator<=(const Real& other) const {
    return *this < other || *this == other;
}

bool Real::operator>(const Real& other) const {
    return !(*this <= other);
}

bool Real::operator>=(const Real& other) const {
    return !(*this < other);
}

Real Real::operator+(const Real& other) const {
    int64_t num = numerator_ * other.denominator_ + other.numerator_ * denominator_;
    int64_t den = denominator_ * other.denominator_;
    return Real(num, den);
}

Real Real::operator-(const Real& other) const {
    int64_t num = numerator_ * other.denominator_ - other.numerator_ * denominator_;
    int64_t den = denominator_ * other.denominator_;
    return Real(num, den);
}

Real Real::operator*(const Real& other) const {
    return Real(numerator_ * other.numerator_, denominator_ * other.denominator_);
}

Real Real::operator/(const Real& other) const {
    if (other.numerator_ == 0) {
        throw std::runtime_error("Division by zero");
    }
    return Real(numerator_ * other.denominator_, denominator_ * other.numerator_);
}

void Real::normalize() {
    if (denominator_ == 0) {
        throw std::runtime_error("Denominator cannot be zero");
    }
    
    // Handle negative denominator
    if (denominator_ < 0) {
        numerator_ = -numerator_;
        denominator_ = -denominator_;
    }
    
    // Find GCD and reduce
    int64_t gcd_val = std::gcd(std::abs(numerator_), std::abs(denominator_));
    if (gcd_val > 1) {
        numerator_ /= gcd_val;
        denominator_ /= gcd_val;
    }
}

} // namespace planmt
