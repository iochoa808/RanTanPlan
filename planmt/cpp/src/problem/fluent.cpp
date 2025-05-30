#include "fluent.h"
#include <sstream>

namespace planmt {

Fluent::Fluent(const pb::Fluent& pb_fluent) 
    : name_(pb_fluent.name()), value_type_(pb_fluent.value_type()) {
    
    for (const auto& pb_param : pb_fluent.parameters()) {
        parameters_.emplace_back(pb_param);
    }
    
    if (pb_fluent.has_default_value()) {
        default_value_ = Expression(pb_fluent.default_value());
    }
}

std::string Fluent::to_string() const {
    std::ostringstream oss;
    oss << name_;
    
    if (!parameters_.empty()) {
        oss << "(";
        for (size_t i = 0; i < parameters_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << parameters_[i].to_string();
        }
        oss << ")";
    }
    
    oss << " : " << value_type_;
    
    if (has_default_value()) {
        oss << " = " << default_value_->to_string();
    }
    
    return oss.str();
}

bool Fluent::operator==(const Fluent& other) const {
    return name_ == other.name_ &&
           value_type_ == other.value_type_ &&
           parameters_ == other.parameters_ &&
           default_value_ == other.default_value_;
}

} // namespace planmt
