#include "plan.hpp"
#include "../util/logger.hpp"
#include <sstream>
#include <fstream>
#include <iomanip>

namespace rantanplan {

void Plan::add_action(const Action* action) {
    if (action) {
        actions_.push_back(action);
    }
}

std::string Plan::to_string() const {
    if (is_empty()) {
        return "Empty plan";
    }

    std::ostringstream oss;
    oss << "Plan with " << length() << " actions:\n";
    for (const Action* action : actions_) {
        oss << action->name() << "\n";
    }

    return oss.str();
}

std::string Plan::to_ipc_string(double cost, bool unit_cost,
                                const std::string& strategy, const std::string& mode,
                                double elapsed_seconds) const {
    std::ostringstream oss;
    oss << std::fixed;

    // Metadata comments
    oss << "; Strategy: " << strategy << "\n";
    if (!mode.empty()) {
        oss << "; Mode: " << mode << "\n";
    }
    oss << std::setprecision(3) << "; Time: " << elapsed_seconds << "s\n";

    // Actions in IPC format: (action_name param1 param2 ...)
    for (const Action* action : actions_) {
        oss << "(" << action->name();
        for (const auto& param : action->parameters()) {
            oss << " " << param.name();
        }
        oss << ")\n";
    }

    // Cost footer
    double plan_cost = (cost >= 0) ? cost : static_cast<double>(length());
    oss << std::setprecision(6)
        << "; cost = " << plan_cost
        << " (" << (unit_cost ? "unit cost" : "general cost") << ")\n";

    return oss.str();
}

std::string Plan::write_ipc(const std::string& base_path, int solution_number,
                            double cost, bool unit_cost,
                            const std::string& strategy, const std::string& mode,
                            double elapsed_seconds) const {
    std::string filename = base_path + "." + std::to_string(solution_number);
    std::ofstream file(filename);
    if (!file.is_open()) {
        Logger::instance().error("Could not write plan to: " + filename);
        return "";
    }
    file << to_ipc_string(cost, unit_cost, strategy, mode, elapsed_seconds);
    Logger::instance().info("Plan written to: " + filename);
    return filename;
}

} // namespace rantanplan