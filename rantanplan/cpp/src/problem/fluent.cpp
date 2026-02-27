#include "fluent.hpp"
#include <sstream>

namespace rantanplan {

Fluent::Fluent(const pb::Fluent& pb_fluent, const Type* value_type, const std::vector<Parameter>& parameters)
    : name_(pb_fluent.name()), value_type_(value_type), parameters_(parameters) {}

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
    oss << " : " << (value_type_ ? value_type_->name() : "null");
    return oss.str();
}

bool Fluent::operator==(const Fluent& other) const {
    return name_ == other.name_ &&
           value_type_ == other.value_type_ &&
           parameters_ == other.parameters_;
}

} // namespace rantanplan
