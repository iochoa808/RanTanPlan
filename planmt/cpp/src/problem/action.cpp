#include "action.h"
#include <sstream>

namespace planmt {

Action::Action(const pb::Action& pb_action, const std::vector<Parameter>& parameters)
    : name_(pb_action.name()), parameters_(parameters) {
    
    // Create single precondition from repeated conditions using AND
    if (pb_action.conditions().empty()) {
        // No preconditions - create true constant
        pb::Expression pb_true_expr;
        pb_true_expr.mutable_atom()->set_boolean(true);
        pb_true_expr.set_kind(pb::ExpressionKind::CONSTANT);
        pb_true_expr.set_type("up:bool");
        precondition_ = Expression(pb_true_expr);
    } else if (pb_action.conditions().size() == 1) {
        // Single precondition - use it directly
        precondition_ = Expression(pb_action.conditions(0).cond());
    } else {
        // Multiple preconditions - create AND expression
        pb::Expression pb_and_expr;
        pb_and_expr.set_kind(pb::ExpressionKind::FUNCTION_APPLICATION);
        pb_and_expr.set_type("up:bool");
        
        // First element: the "and" function symbol
        pb::Expression* and_symbol = pb_and_expr.add_list();
        and_symbol->mutable_atom()->set_symbol("and");
        and_symbol->set_kind(pb::ExpressionKind::FUNCTION_SYMBOL);
        and_symbol->set_type("up:bool");
        
        // Add all the condition expressions directly from protobuf
        for (const auto& pb_condition : pb_action.conditions()) {
            pb::Expression* operand = pb_and_expr.add_list();
            *operand = pb_condition.cond();
        }
        
        precondition_ = Expression(pb_and_expr);
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
    
    if (has_precondition()) {
        oss << "\n  Precondition: " << precondition_.to_string();
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
           precondition_ == other.precondition_ &&
           effects_ == other.effects_;
}

void Action::build_parameter_mappings() {
    parameter_name_to_index_.clear();
    for (size_t i = 0; i < parameters_.size(); ++i) {
        parameter_name_to_index_[parameters_[i].name()] = i;
    }
}

} // namespace planmt
