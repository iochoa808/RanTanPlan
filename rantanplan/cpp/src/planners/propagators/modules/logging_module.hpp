#pragma once

#include "../propagator_module.hpp"
#include "../propagator_strategy.hpp"
#include <fstream>
#include <string>

namespace rantanplan {

/**
 * @brief Solver decision logging module.
 *
 * When enabled, registers all fluent and action variables with Z3 and
 * logs solver decisions (dec), timestep increments (inc), and restarts
 * to a file.  Requires the Z3 decide callback — only registered when
 * this module is present.
 */
class LoggingModule : public PropagatorModule {
public:
    LoggingModule(const std::string& log_file_path,
                  const std::string& strategy_name);

    std::string get_name() const override { return "Logging"; }
    bool wants_decide_callback() const override { return true; }

    void initialize(PropagatorSharedState& shared,
                    PropagatorStrategy& host) override;
    void register_timestep_variables(int timestep) override;
    void on_push() override;
    void on_pop(unsigned num_scopes) override;
    void on_decide(const z3::expr& val, unsigned bit, bool is_pos) override;

private:
    std::ofstream log_file_;
    std::string strategy_name_;

    // Restart detection
    int current_decision_level_ = 0;
    int max_decision_level_seen_ = 0;
};

} // namespace rantanplan
