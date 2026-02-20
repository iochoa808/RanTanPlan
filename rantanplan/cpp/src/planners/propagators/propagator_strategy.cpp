#include "propagator_strategy.hpp"
#include "../../encoders/z3_variable_factory.hpp"

namespace rantanplan {

PropagatorStrategy::PropagatorStrategy(z3::solver& solver, const BaseEncoder& encoder)
    : z3::user_propagator_base(&solver), encoder_(&encoder) {
    // Register Z3 callbacks that all propagators need
    register_fixed();
    register_final();
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

void PropagatorStrategy::enable_logging(const std::string& log_file_path,
                                         const std::string& strategy_name) {
    log_file_.open(log_file_path);
    if (log_file_.is_open()) {
        logging_enabled_ = true;
        strategy_name_ = strategy_name;
        log_file_ << "# rantanplan solver log | strategy: " << strategy_name << "\n";
        // Enable the decide callback so we get notified of solver decisions
        register_decide();
    }
}

// ---------------------------------------------------------------------------
// register_timestep_variables — base implementation
// ---------------------------------------------------------------------------

void PropagatorStrategy::register_timestep_variables(int timestep) {
    if (logging_enabled_) {
        log_file_ << "inc\n";
        // Reset restart detection state at SAT-call boundary
        current_decision_level_ = 0;
        max_decision_level_seen_ = 0;
    }
    register_all_variables_for_logging(timestep);
}

void PropagatorStrategy::register_all_variables_for_logging(int timestep) {
    if (!logging_enabled_) return;

    const Z3VariableFactory& vf = encoder_->get_variable_factory();

    // Register fluent (state) variables for this timestep
    auto fluent_vars = vf.get_all_fluent_variables(timestep);
    for (const auto& var_ptr : fluent_vars) {
        add(*var_ptr);
    }

    // Register action variables for the previous timestep (actions at t cause
    // transition from state t to state t+1; they are created when
    // encode_actions(t) is called, which happens before
    // register_timestep_variables(t+1)).
    if (timestep > 0) {
        auto action_vars = vf.get_all_action_variables(timestep - 1);
        for (const auto& var_ptr : action_vars) {
            add(*var_ptr);
        }
    }
}

// ---------------------------------------------------------------------------
// Z3 callbacks — delegate to on_* hooks after base-level bookkeeping
// ---------------------------------------------------------------------------

void PropagatorStrategy::push() {
    current_decision_level_++;
    if (current_decision_level_ > max_decision_level_seen_) {
        max_decision_level_seen_ = current_decision_level_;
    }
    on_push();
}

void PropagatorStrategy::pop(unsigned num_scopes) {
    current_decision_level_ -= static_cast<int>(num_scopes);

    // Restart detection: pop back to level 0 after having been deeper
    if (logging_enabled_ && current_decision_level_ == 0 && max_decision_level_seen_ > 1) {
        log_file_ << "restart\n";
        max_decision_level_seen_ = 0;
    }

    on_pop(num_scopes);
}

void PropagatorStrategy::fixed(z3::expr const& ast, z3::expr const& value) {
    on_fixed(ast, value);
}

void PropagatorStrategy::decide(z3::expr const& val, unsigned bit, bool is_pos) {
    if (logging_enabled_) {
        const char* sign = is_pos ? "+" : "-";
        // Arity-0 = pure named variable; otherwise compound expression
        if (val.is_app() && val.decl().arity() == 0) {
            log_file_ << "dec " << sign << val.decl().name().str() << "\n";
        } else {
            // Z3's to_string() can produce multi-line output for complex
            // expressions; collapse to a single line for the log format
            std::string s = val.to_string();
            for (char& c : s) {
                if (c == '\n') c = ' ';
            }
            log_file_ << "dec " << sign << s << "\n";
        }
    }
    on_decide(val, bit, is_pos);
}

void PropagatorStrategy::final() {
    on_final();
}

z3::user_propagator_base* PropagatorStrategy::fresh(z3::context& ctx) {
    return on_fresh(ctx);
}

} // namespace rantanplan
