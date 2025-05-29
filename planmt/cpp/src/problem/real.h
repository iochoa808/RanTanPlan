#pragma once

#include <cstdint>
#include <string>
#include "protobuf_aliases.h"

namespace planmt {

/**
 * @brief Simple real number representation
 * 
 * Represents a rational number as numerator/denominator fraction.
 * Much simpler than protobuf Real - focuses on essential functionality.
 */
class Real {
public:
    // Constructors
    Real() : numerator_(0), denominator_(1) {}
    Real(int64_t numerator, int64_t denominator = 1) : numerator_(numerator), denominator_(denominator) {}
    Real(double value);
    Real(const pb::Real& pb_real);
    
    // Accessors
    int64_t numerator() const { return numerator_; }
    int64_t denominator() const { return denominator_; }
    
    // Conversion
    double to_double() const;
    std::string to_string() const;
    
    // Convert to protobuf Real
    pb::Real to_protobuf() const;
    
    // Operators
    bool operator==(const Real& other) const;
    bool operator!=(const Real& other) const { return !(*this == other); }
    bool operator<(const Real& other) const;
    bool operator<=(const Real& other) const;
    bool operator>(const Real& other) const;
    bool operator>=(const Real& other) const;
    
    Real operator+(const Real& other) const;
    Real operator-(const Real& other) const;
    Real operator*(const Real& other) const;
    Real operator/(const Real& other) const;

private:
    int64_t numerator_;
    int64_t denominator_;
    
    void normalize(); // Reduce to lowest terms
};

} // namespace planmt
