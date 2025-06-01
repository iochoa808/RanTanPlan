#include "action.h"
#include <sstream>

namespace planmt {

Action::Action(const pb::Action& pb_action, const std::vector<Parameter>& parameters)
    : name_(pb_action.name()), parameters_(parameters) {
    for (const auto& pb_precond : pb_action.conditions()) {
        preconditions_.emplace_back(pb_precond.cond());
    }
    
    for (const auto& pb_effect : pb_action.effects()) {
        effects_.emplace_back(pb_effect);
    }
    
    build_parameter_mappings();
}

bool Action::has_parameter(const std::string& name) const {
    return parameter_name_to_index_.find(name) != parameter_name_to_index_.end();
}

const Parameter* Action::find_parameter(const std::string& name) const {
    auto it = parameter_name_to_index_.find(name);
    if (it != parameter_name_to_index_.end()) {
        return &parameters_[it->second];
    }
    return nullptr;
}

void Action::add_parameter(const Parameter& param) {
    parameters_.push_back(param);
    build_parameter_mappings();
}

void Action::set_parameters(const std::vector<Parameter>& parameters) {
    parameters_ = parameters;
    build_parameter_mappings();
}

std::string Action::to_string() const {
    std::ostringstream oss;
    oss << "Action: " << name_;
    
    if (!parameters_.empty()) {
        oss << "(";
        for (size_t i = 0; i < parameters_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << parameters_[i].to_string();
        }
        oss << ")";
    }
    
    if (!preconditions_.empty()) {
        oss << "\n  Preconditions:";
        for (const auto& precond : preconditions_) {
            oss << "\n    " << precond.to_string();
        }
    }
    
    if (!effects_.empty()) {
        oss << "\n  Effects:";
        for (const auto& effect : effects_) {
            oss << "\n    " << effect.to_string();
        }
    }
    
    return oss.str();
}

bool Action::operator==(const Action& other) const {
    return name_ == other.name_ &&
           parameters_ == other.parameters_ &&
           preconditions_ == other.preconditions_ &&
           effects_ == other.effects_;
}

void Action::build_parameter_mappings() {
    parameter_name_to_index_.clear();
    for (size_t i = 0; i < parameters_.size(); ++i) {
        parameter_name_to_index_[parameters_[i].name()] = i;
    }
}

} // namespace planmt
