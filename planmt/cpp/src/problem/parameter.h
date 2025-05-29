#pragma once

#include <string>
#include "protobuf_aliases.h"

namespace planmt {

/**
 * @brief Simple parameter representation
 * 
 * Represents a typed parameter for actions or fluents.
 * Much simpler than protobuf Parameter - focuses on essential functionality.
 */
class Parameter {
public:
    // Constructors
    Parameter() = default;
    Parameter(const std::string& name, const std::string& type) 
        : name_(name), type_(type) {}
    Parameter(const pb::Parameter& pb_parameter);
    
    // Accessors
    const std::string& name() const { return name_; }
    const std::string& type() const { return type_; }
    
    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void set_type(const std::string& type) { type_ = type; }
    
    // String representation
    std::string to_string() const { return name_ + " : " + type_; }
    
    // Convert to protobuf Parameter
    pb::Parameter to_protobuf() const;
    
    // Operators
    bool operator==(const Parameter& other) const {
        return name_ == other.name_ && type_ == other.type_;
    }
    bool operator!=(const Parameter& other) const { return !(*this == other); }

private:
    std::string name_;
    std::string type_;
};

} // namespace planmt
