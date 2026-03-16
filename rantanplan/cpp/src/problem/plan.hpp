#pragma once

#include <vector>
#include <string>
#include "action.hpp"

namespace rantanplan {

/**
 * @brief Plan representation
 *
 * Represents a plan as a simple sequence of actions.
 */
class Plan {
public:
    // Constructors
    Plan() = default;

    // Add an action to the end of the plan
    void add_action(const Action* action);

    // Access actions
    const std::vector<const Action*>& actions() const { return actions_; }

    // Basic properties
    size_t length() const { return actions_.size(); }
    bool is_empty() const { return actions_.empty(); }

    // String representation for debugging
    std::string to_string() const;

    // Clear the plan
    void clear() { actions_.clear(); }

    /// Format plan as IPC string with metadata comments and cost footer.
    /// cost < 0 means no cost info (unit cost footer omitted or set to action count).
    std::string to_ipc_string(double cost, bool unit_cost,
                              const std::string& strategy, const std::string& mode,
                              double elapsed_seconds) const;

    /// Write plan to file in IPC format. Appends ".N" to base_path.
    /// Returns the full path written, or empty string on failure.
    std::string write_ipc(const std::string& base_path, int solution_number,
                          double cost, bool unit_cost,
                          const std::string& strategy, const std::string& mode,
                          double elapsed_seconds) const;

private:
    // A simple sequence of actions
    std::vector<const Action*> actions_;
};

} // namespace rantanplan