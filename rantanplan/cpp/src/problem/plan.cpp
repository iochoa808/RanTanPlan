#include "plan.hpp"
#include <sstream>

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

} // namespace rantanplan