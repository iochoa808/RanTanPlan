#include "object_fluent_index.hpp"
#include <cassert>

namespace rantanplan {

// Static empty containers for out-of-range queries.
const std::vector<std::vector<int>> ObjectFluentIndex::empty_tuples_;
const std::unordered_set<int> ObjectFluentIndex::empty_values_;

ObjectFluentIndex::ObjectFluentIndex(const Problem& problem)
    : problem_(problem)
{
    size_t num_fluents = problem_.fluent_count();
    by_fluent_.resize(num_fluents);
    extended_tuples_.resize(num_fluents);
}

void ObjectFluentIndex::initialize_from_initial_state() {
    const auto& pool = problem_.pool();

    for (const auto& assignment : problem_.initial_state()) {
        ExprID fluent_eid = assignment.fluent_id();
        ExprID value_eid  = assignment.value_id();

        int schema_id = -1;
        std::vector<int> arg_indices;
        if (!decompose_object_fluent(fluent_eid, schema_id, arg_indices)) {
            continue;  // Not an object fluent or decomposition failed.
        }

        int value_obj = -1;
        if (!resolve_value_object(value_eid, value_obj)) {
            continue;  // Value isn't a ground object constant.
        }

        add_value(schema_id, arg_indices, value_obj);
    }
}

bool ObjectFluentIndex::add_value(int fluent_schema_id,
                                   const std::vector<int>& arg_obj_indices,
                                   int value_obj_index) {
    assert(fluent_schema_id >= 0 &&
           static_cast<size_t>(fluent_schema_id) < by_fluent_.size());

    auto& [values_map] = by_fluent_[fluent_schema_id];
    auto& value_set = values_map[arg_obj_indices];

    if (!value_set.insert(value_obj_index).second) {
        return false;  // Already known.
    }

    // Build and append the extended tuple [arg0, ..., argN, value].
    std::vector<int> tuple = arg_obj_indices;
    tuple.push_back(value_obj_index);
    extended_tuples_[fluent_schema_id].push_back(std::move(tuple));

    ++total_entries_;
    return true;
}

const std::vector<std::vector<int>>& ObjectFluentIndex::get_extended_tuples(
    int fluent_schema_id) const
{
    if (fluent_schema_id < 0 ||
        static_cast<size_t>(fluent_schema_id) >= extended_tuples_.size()) {
        return empty_tuples_;
    }
    return extended_tuples_[fluent_schema_id];
}

const std::unordered_set<int>& ObjectFluentIndex::get_values(
    int fluent_schema_id,
    const std::vector<int>& arg_obj_indices) const
{
    if (fluent_schema_id < 0 ||
        static_cast<size_t>(fluent_schema_id) >= by_fluent_.size()) {
        return empty_values_;
    }
    const auto& map = by_fluent_[fluent_schema_id].values;
    auto it = map.find(arg_obj_indices);
    if (it != map.end()) {
        return it->second;
    }
    return empty_values_;
}

bool ObjectFluentIndex::decompose_object_fluent(ExprID eid,
                                                  int& out_schema_id,
                                                  std::vector<int>& out_arg_indices) const {
    const auto& pool = problem_.pool();

    if (!pool.is_state_variable(eid)) return false;

    ExprID head_id = pool.head_symbol_id(eid);
    if (!pool.is_fluent_symbol(head_id)) return false;

    const std::string& fluent_name = pool.payload_string(head_id);
    const Fluent* fluent = problem_.find_fluent(fluent_name);
    if (!fluent) return false;

    // Only object fluents (value type is an object type).
    if (!fluent->is_object_fluent()) return false;

    out_schema_id = fluent->id();

    size_t num_args = pool.argument_count(eid);
    out_arg_indices.clear();
    out_arg_indices.reserve(num_args);

    for (size_t i = 0; i < num_args; ++i) {
        ExprID arg_id = pool.argument(eid, i);

        if (!pool.is_constant(arg_id) || !pool.payload_is_string(arg_id)) {
            return false;  // Not ground (PARAMETER node or non-string).
        }

        const std::string& obj_name = pool.payload_string(arg_id);
        const Object* obj = problem_.find_object(obj_name);
        if (!obj) return false;

        size_t obj_index = static_cast<size_t>(obj - &problem_.objects()[0]);
        out_arg_indices.push_back(static_cast<int>(obj_index));
    }

    return true;
}

bool ObjectFluentIndex::resolve_value_object(ExprID value_eid,
                                               int& out_obj_index) const {
    const auto& pool = problem_.pool();

    if (!pool.is_constant(value_eid) || !pool.payload_is_string(value_eid)) {
        return false;
    }

    const std::string& obj_name = pool.payload_string(value_eid);
    const Object* obj = problem_.find_object(obj_name);
    if (!obj) return false;

    out_obj_index = static_cast<int>(obj - &problem_.objects()[0]);
    return true;
}

} // namespace rantanplan
