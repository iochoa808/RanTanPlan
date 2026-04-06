#include "logging_module.hpp"
#include "../../../encoders/z3_variable_factory.hpp"

namespace rantanplan {

LoggingModule::LoggingModule(const std::string& log_file_path,
                             const std::string& strategy_name)
    : strategy_name_(strategy_name) {
    log_file_.open(log_file_path);
    if (log_file_.is_open()) {
        log_file_ << "# rantanplan solver log | strategy: " << strategy_name << "\n";
    }
}

void LoggingModule::initialize(PropagatorSharedState& shared,
                               PropagatorStrategy& host) {
    PropagatorModule::initialize(shared, host);
}

void LoggingModule::register_timestep_variables(int timestep) {
    if (!log_file_.is_open()) return;

    log_file_ << "inc\n";
    current_decision_level_ = 0;
    max_decision_level_seen_ = 0;

    const Z3VariableFactory& vf = *shared_->variable_factory;

    // Register fluent variables for this timestep
    auto fluent_vars = vf.get_all_fluent_variables(timestep);
    for (const auto& var_ptr : fluent_vars)
        host_->module_add(*var_ptr);

    // Register action variables for the previous timestep
    if (timestep > 0) {
        auto action_vars = vf.get_all_action_variables(timestep - 1);
        for (const auto& var_ptr : action_vars)
            host_->module_add(*var_ptr);
    }
}

void LoggingModule::on_push() {
    current_decision_level_++;
    if (current_decision_level_ > max_decision_level_seen_)
        max_decision_level_seen_ = current_decision_level_;
}

void LoggingModule::on_pop(unsigned num_scopes) {
    current_decision_level_ -= static_cast<int>(num_scopes);
    if (log_file_.is_open() && current_decision_level_ == 0 && max_decision_level_seen_ > 1) {
        log_file_ << "restart\n";
        max_decision_level_seen_ = 0;
    }
}

void LoggingModule::on_decide(const z3::expr& val, unsigned /*bit*/, bool is_pos) {
    if (!log_file_.is_open()) return;

    const char* sign = is_pos ? "+" : "-";
    if (val.is_app() && val.decl().arity() == 0) {
        log_file_ << "dec " << sign << val.decl().name().str() << "\n";
    } else {
        std::string s = val.to_string();
        for (char& c : s) {
            if (c == '\n') c = ' ';
        }
        log_file_ << "dec " << sign << s << "\n";
    }
}

} // namespace rantanplan
