#include "atom.hpp"

namespace rantanplan {

std::string Atom::to_string() const {
    return std::visit([](const auto& value) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
            return value;
        } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int64_t>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Real>) {
            return value.to_string();
        } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, bool>) {
            return value ? "true" : "false";
        }
        return "";
    }, value_);
}

} // namespace rantanplan
