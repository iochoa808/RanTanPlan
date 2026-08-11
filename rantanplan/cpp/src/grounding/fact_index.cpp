#include "fact_index.hpp"
#include <cassert>

namespace rantanplan {

// Static empty vector for out-of-range get_facts() calls.
const std::vector<std::vector<int>> FactIndex::empty_facts_;

FactIndex::FactIndex(const Problem& problem)
    : problem_(problem)
{
    // Allocate one bucket per fluent schema.
    size_t num_fluents = problem_.fluent_count();
    facts_by_fluent_.resize(num_fluents);
    fact_sets_.resize(num_fluents);
}

void FactIndex::initialize_from_initial_state() {
    // Walk all initial-state assignments. For each boolean predicate assigned
    // to `true`, decompose the fluent application into (schema_id, object_indices)
    // and insert into the index.
    //
    // We skip:
    //   - Numeric assignments (they don't produce boolean facts)
    //   - Boolean assignments to `false` (closed-world: unlisted = false)
    //   - Anything we can't decompose (shouldn't happen for well-formed input)

    const auto& pool = problem_.pool();

    for (const auto& assignment : problem_.initial_state()) {
        ExprID fluent_eid = assignment.fluent_id();
        ExprID value_eid = assignment.value_id();

        // Only boolean predicates assigned to true are positive facts.
        if (!pool.is_true_constant(value_eid)) continue;

        int schema_id = -1;
        std::vector<int> obj_indices;
        if (decompose_state_variable(fluent_eid, schema_id, obj_indices)) {
            add_fact(schema_id, obj_indices);
        }
    }
}

bool FactIndex::add_fact(int fluent_schema_id, const std::vector<int>& object_indices) {
    assert(fluent_schema_id >= 0 &&
           static_cast<size_t>(fluent_schema_id) < facts_by_fluent_.size());

    auto& fact_set = fact_sets_[fluent_schema_id];

    // O(1) amortized check using full tuple equality (not hash-only).
    if (!fact_set.insert(object_indices).second) {
        return false;  // already known
    }

    // New fact — store the tuple for enumeration.
    facts_by_fluent_[fluent_schema_id].push_back(object_indices);
    ++total_facts_;
    return true;
}

bool FactIndex::contains(int fluent_schema_id, const std::vector<int>& object_indices) const {
    if (fluent_schema_id < 0 ||
        static_cast<size_t>(fluent_schema_id) >= fact_sets_.size()) {
        return false;
    }
    return fact_sets_[fluent_schema_id].count(object_indices) > 0;
}

const std::vector<std::vector<int>>& FactIndex::get_facts(int fluent_schema_id) const {
    if (fluent_schema_id < 0 ||
        static_cast<size_t>(fluent_schema_id) >= facts_by_fluent_.size()) {
        return empty_facts_;
    }
    return facts_by_fluent_[fluent_schema_id];
}

bool FactIndex::decompose_state_variable(ExprID eid,
                                          int& out_fluent_schema_id,
                                          std::vector<int>& out_object_indices) const {
    const auto& pool = problem_.pool();

    // A STATE_VARIABLE's ExprPool layout:
    //   children[0] = FLUENT_SYMBOL  (payload_string = fluent name)
    //   children[1..] = arguments    (CONSTANT, payload_string = object name)
    if (!pool.is_state_variable(eid)) return false;

    // Get the fluent symbol (head).
    ExprID head_id = pool.head_symbol_id(eid);
    if (!pool.is_fluent_symbol(head_id)) return false;

    const std::string& fluent_name = pool.payload_string(head_id);

    // Look up the fluent schema by name to get its ID.
    const Fluent* fluent = problem_.find_fluent(fluent_name);
    if (!fluent) return false;

    // Only boolean predicates produce facts in the FactIndex.
    if (!fluent->is_predicate()) return false;

    out_fluent_schema_id = fluent->id();

    // Decompose arguments to object indices.
    size_t num_args = pool.argument_count(eid);
    out_object_indices.clear();
    out_object_indices.reserve(num_args);

    for (size_t i = 0; i < num_args; ++i) {
        ExprID arg_id = pool.argument(eid, i);

        if (!pool.is_constant(arg_id)) {
            // PARAMETER node (lifted) — that's not a ground fact.
            return false;
        }

        if (pool.payload_is_string(arg_id)) {
            const std::string& obj_name = pool.payload_string(arg_id);
            const Object* obj = problem_.find_object(obj_name);
            if (!obj) return false;

            // Find object index by scanning (objects are stored by index).
            // Problem::find_object returns a pointer; derive index from address.
            size_t obj_index = static_cast<size_t>(obj - &problem_.objects()[0]);
            out_object_indices.push_back(static_cast<int>(obj_index));
        } else if (pool.payload_is_int(arg_id)) {
            // Integer-typed element (e.g. membership predicate for set-of-int).
            // The integer value is used directly as the "index", consistent with
            // BindingMatcher::get_type_compatible_objects for bounded-int params.
            out_object_indices.push_back(static_cast<int>(pool.payload_int(arg_id)));
        } else {
            return false;
        }
    }

    return true;
}

} // namespace rantanplan
