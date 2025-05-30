#pragma once

#include <string>
#include "protobuf_aliases.h"

namespace planmt {

/**
 * @brief Object declaration
 * 
 * Represents a typed object in the planning domain.
 */
class Object {
public:
    // Constructors
    Object() = default;
    Object(const std::string& name, const std::string& type) 
        : name_(name), type_(type) {}
    Object(const pb::ObjectDeclaration& pb_object);
    
    // Accessors
    const std::string& name() const { return name_; }
    const std::string& type() const { return type_; }
    
    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void set_type(const std::string& type) { type_ = type; }
    
    // String representation
    std::string to_string() const { return name_ + " : " + type_; }
    
    // Convert to protobuf ObjectDeclaration
    pb::ObjectDeclaration to_protobuf() const;
    
    // Operators
    bool operator==(const Object& other) const {
        return name_ == other.name_ && type_ == other.type_;
    }
    bool operator!=(const Object& other) const { return !(*this == other); }

private:
    std::string name_;
    std::string type_;
};

} // namespace planmt
