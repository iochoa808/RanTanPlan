#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "protobuf_aliases.hpp"
#include "parameter.hpp"
#include "effect.hpp"
#include "expr_pool.hpp"

namespace rantanplan {

/**
 * @brief Action
 *
 * Represents a planning action with parameters, preconditions, and effects.
 */
class Action {
public:
    // Constructors
    Action() = default;
    Action(const std::string& name) : name_(name), id_(-1) {}
    Action(const pb::Action& pb_action, const std::vector<Parameter>& parameters, Problem* problem);

    // Basic accessors
    const std::string& name() const { return name_; }
    int id() const { return id_; }

    // Parameter access
    const std::vector<Parameter>& parameters() const { return parameters_; }
    size_t parameter_count() const { return parameters_.size(); }
    const Parameter& parameter(size_t index) const { return parameters_[index]; }
    bool has_parameter(const std::string& name) const;
    const Parameter* find_parameter(const std::string& name) const;

    // Precondition access
    ExprID precondition_id() const { return precondition_id_; }
    bool has_precondition() const { return has_precondition_; }

    // Effect access
    const std::vector<Effect>& effects() const { return effects_; }
    std::vector<Effect>& mutable_effects() { return effects_; }
    size_t effect_count() const { return effects_.size(); }
    const Effect& effect(size_t index) const { return effects_[index]; }

    // Setters (used by Problem internals)
    void set_id(int id) { id_ = id; }
    void add_parameter(const Parameter& param);
    void set_parameters(const std::vector<Parameter>& parameters);
    void add_effect(const Effect& effect) { effects_.push_back(effect); }
    void set_precondition_id(ExprID id) { precondition_id_ = id; has_precondition_ = (id != EXPR_NULL); }
    void set_pool(const ExprPool* pool) { pool_ = pool; }

    // String representation
    std::string to_string() const;

    // Operators
    bool operator==(const Action& other) const;
    bool operator!=(const Action& other) const { return !(*this == other); }
    bool operator<(const Action& other) const;

private:
    std::string name_;
    int id_ = -1;
    std::vector<Parameter> parameters_;
    ExprID precondition_id_ = EXPR_NULL;
    bool has_precondition_ = false;
    std::vector<Effect> effects_;
    const ExprPool* pool_ = nullptr; // for to_string()

    // Quick lookup for parameters
    std::unordered_map<std::string, size_t> parameter_name_to_index_;

    void build_parameter_mappings();
};

} // namespace rantanplan

// specialising std::hash for Action by using its ID
// note that for this to work we need to be in the std namespace
namespace std {
    template<>
    struct hash<rantanplan::Action> {
        std::size_t operator()(const rantanplan::Action& action) const {
            return std::hash<int>()(action.id());
        }
    };
}
