#include "problem.hpp"
#include "visitors/fluent_collector.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace rantanplan {

// ============================================================================
// Expression interning
// ============================================================================

ExprID Problem::intern_from_protobuf(const pb::Expression& pb_expr) {
    ExprNode node;

    // Convert kind
    int kind_value = static_cast<int>(pb_expr.kind());
    if (kind_value == 8) {
        throw std::runtime_error("Unsupported expression kind CONTAINER_ID (8) in protobuf input");
    }
    node.kind = kind_value;

    // Resolve type string to type_id
    const std::string& type_str = pb_expr.type();
    if (!type_str.empty()) {
        std::string resolved = type_str;
        if (type_str == "up:integer") resolved = "up:int";
        else if (type_str == "up:boolean") resolved = "up:bool";

        const Type* found_type = find_type(resolved);
        if (!found_type && resolved != type_str) {
            found_type = find_type(type_str);
        }
        if (found_type) {
            for (size_t i = 0; i < types_->size(); ++i) {
                if (&(*types_)[i] == found_type) {
                    node.type_id = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    if (pb_expr.has_atom()) {
        // Atom expression — extract payload
        const auto& pb_atom = pb_expr.atom();
        ExprKind kind = static_cast<ExprKind>(pb_expr.kind());

        if (pb_atom.has_symbol()) {
            std::string symbol = pb_atom.symbol();
            // Apply UP operator mapping for function/fluent symbols
            if (kind == ExprKind::FUNCTION_SYMBOL || kind == ExprKind::FLUENT_SYMBOL) {
                symbol = map_up_operator(symbol);
            }
            node.payload = symbol;
            // Extract operator for function symbols
            if (kind == ExprKind::FUNCTION_SYMBOL) {
                node.op = static_cast<int>(string_to_expr_operator(symbol));
            }
        } else if (pb_atom.has_int_()) {
            node.payload = pb_atom.int_();
        } else if (pb_atom.has_real()) {
            double val = static_cast<double>(pb_atom.real().numerator()) /
                         static_cast<double>(pb_atom.real().denominator());
            node.payload = val;
        } else if (pb_atom.has_boolean()) {
            node.payload = pb_atom.boolean();
        }
    }

    if (pb_expr.list_size() > 0) {
        // Compound expression — recursively intern children
        ExprKind kind = static_cast<ExprKind>(pb_expr.kind());

        node.children.reserve(pb_expr.list_size());
        for (int i = 0; i < pb_expr.list_size(); ++i) {
            const auto& child_pb = pb_expr.list(i);

            // For the first child in function applications, apply operator mapping
            if (i == 0 && kind == ExprKind::FUNCTION_APPLICATION &&
                child_pb.has_atom() && child_pb.atom().has_symbol()) {
                // Create a copy with mapped operator symbol
                pb::Expression mapped_child = child_pb;
                std::string mapped = map_up_operator(child_pb.atom().symbol());
                mapped_child.mutable_atom()->set_symbol(mapped);
                ExprID child_id = intern_from_protobuf(mapped_child);
                node.children.push_back(child_id);

                // Extract operator from first child
                node.op = static_cast<int>(string_to_expr_operator(mapped));
            } else {
                node.children.push_back(intern_from_protobuf(child_pb));
            }
        }
    }

    return pool_->intern(std::move(node));
}

void Problem::collect_grounded_fluents() {
    // Collect ALL unique grounded fluent applications from:
    //   1. Initial state assignments
    //   2. Action preconditions and effects
    //   3. Goals
    // This ensures every fluent that could appear in the encoding gets
    // a frame axiom and proper initial-state treatment.
    FluentCollector collector(*this);

    // 1. Initial state
    for (const auto& assignment : initial_state_) {
        collector.collect_from_id(assignment.fluent_id());
    }

    // 2. Actions: preconditions + effect fluents, values, and conditions
    for (const auto& action : actions_) {
        if (action.has_precondition()) {
            collector.collect_from_id(action.precondition_id());
        }
        for (const auto& effect : action.effects()) {
            const auto& ee = effect.effect_expression();
            collector.collect_from_id(ee.fluent_id());
            collector.collect_from_id(ee.value_id());
            if (ee.is_conditional()) {
                collector.collect_from_id(ee.condition_id());
            }
        }
    }

    // 3. Goals
    for (const auto& goal : goals_) {
        collector.collect_from_id(goal.goal_id());
    }

    // Build the deduped list, sorted by ExprID for deterministic ordering.
    // FluentCollector uses an unordered_set, so iteration order is
    // non-deterministic; sorting ensures reproducible variable ordering
    // across runs and platforms.
    grounded_fluents_.clear();
    grounded_fluents_.reserve(collector.fluent_count());
    for (ExprID eid : collector.get_fluents()) {
        grounded_fluents_.push_back(eid);
    }
    std::sort(grounded_fluents_.begin(), grounded_fluents_.end());

    build_grounded_fluent_mappings();
}

Problem::Problem(const pb::Problem& pb_problem) {
    load_types(pb_problem.types());
    resolve_type_hierarchy();

    // Load objects
    load_objects(pb_problem.objects());

    // Load fluents
    load_fluents(pb_problem.fluents());

    // Load actions (interns expressions via intern_from_protobuf)
    load_actions(pb_problem.actions());

    // Load initial state (interns expressions via intern_from_protobuf)
    for (const auto& pb_assignment : pb_problem.initial_state()) {
        initial_state_.emplace_back(pb_assignment, this);
    }

    // Load goals (interns expressions via intern_from_protobuf)
    for (const auto& pb_goal : pb_problem.goals()) {
        goals_.emplace_back(pb_goal, this);
    }

    // Collect grounded fluents from initial state assignments
    collect_grounded_fluents();
}

bool Problem::has_object(const std::string& name) const {
    return object_name_to_index_.find(name) != object_name_to_index_.end();
}

const Object* Problem::find_object(const std::string& name) const {
    auto it = object_name_to_index_.find(name);
    if (it != object_name_to_index_.end()) {
        return &objects_[it->second];
    }
    return nullptr;
}

bool Problem::has_fluent(const std::string& name) const {
    return fluent_name_to_index_.find(name) != fluent_name_to_index_.end();
}

const Fluent* Problem::find_fluent(const std::string& name) const {
    auto it = fluent_name_to_index_.find(name);
    if (it != fluent_name_to_index_.end()) {
        return &fluents_[it->second];
    }
    return nullptr;
}

bool Problem::has_action(const std::string& name) const {
    return action_name_to_index_.find(name) != action_name_to_index_.end();
}

const Action* Problem::find_action(const std::string& name) const {
    auto it = action_name_to_index_.find(name);
    if (it != action_name_to_index_.end()) {
        return &actions_[it->second];
    }
    return nullptr;
}

const Type* Problem::find_type(const std::string& name) const {
    auto it = type_name_to_ptr_.find(name);
    if (it != type_name_to_ptr_.end()) return it->second;
    return nullptr;
}

Problem Problem::without_actions(const std::vector<size_t>& removed_indices) const {
    std::unordered_set<size_t> removed_set(removed_indices.begin(), removed_indices.end());

    Problem result;
    result.domain_name_ = domain_name_;
    result.problem_name_ = problem_name_;
    result.types_ = types_;               // shared_ptr — same vector, Type* stay valid
    result.pool_ = pool_;                 // shared_ptr — same ExprPool
    result.objects_ = objects_;            // Type* point into shared types_ — valid
    result.fluents_ = fluents_;            // Type* point into shared types_ — valid
    result.grounded_fluents_ = grounded_fluents_;
    result.initial_state_ = initial_state_;
    result.goals_ = goals_;

    // Copy all index maps except action (which needs rebuilding)
    result.object_name_to_index_ = object_name_to_index_;
    result.fluent_name_to_index_ = fluent_name_to_index_;
    result.grounded_fluent_to_index_ = grounded_fluent_to_index_;
    result.type_name_to_ptr_ = type_name_to_ptr_;  // pointers into shared types_ — valid

    // Filter actions, re-index IDs
    result.actions_.reserve(actions_.size() - removed_set.size());
    int new_id = 0;
    for (size_t i = 0; i < actions_.size(); ++i) {
        if (!removed_set.count(i)) {
            result.actions_.push_back(actions_[i]);
            result.actions_.back().set_id(new_id++);
        }
    }
    result.build_action_mappings();

    return result;
}

Problem Problem::with_actions(std::vector<Action> new_actions) const {
    Problem result;
    result.domain_name_ = domain_name_;
    result.problem_name_ = problem_name_;
    result.types_ = types_;
    result.pool_ = pool_;
    result.objects_ = objects_;
    result.fluents_ = fluents_;
    result.initial_state_ = initial_state_;
    result.goals_ = goals_;

    // Copy index maps (except action and grounded_fluent — those are rebuilt)
    result.object_name_to_index_ = object_name_to_index_;
    result.fluent_name_to_index_ = fluent_name_to_index_;
    result.type_name_to_ptr_ = type_name_to_ptr_;

    // Set new actions with contiguous IDs
    result.actions_ = std::move(new_actions);
    for (size_t i = 0; i < result.actions_.size(); ++i) {
        result.actions_[i].set_id(static_cast<int>(i));
    }
    result.build_action_mappings();

    // Rebuild grounded fluents from the new action set (+ initial state).
    // collect_grounded_fluents() calls build_grounded_fluent_mappings() internally.
    result.collect_grounded_fluents();

    return result;
}

Problem Problem::with_additional_initial_state(const std::vector<Assignment>& extra_assignments) const {
    if (extra_assignments.empty()) return *this;

    Problem result;
    result.domain_name_ = domain_name_;
    result.problem_name_ = problem_name_;
    result.types_ = types_;
    result.pool_ = pool_;
    result.objects_ = objects_;
    result.fluents_ = fluents_;
    result.actions_ = actions_;
    result.grounded_fluents_ = grounded_fluents_;
    result.goals_ = goals_;

    // Copy all index maps
    result.object_name_to_index_ = object_name_to_index_;
    result.fluent_name_to_index_ = fluent_name_to_index_;
    result.action_name_to_index_ = action_name_to_index_;
    result.grounded_fluent_to_index_ = grounded_fluent_to_index_;
    result.type_name_to_ptr_ = type_name_to_ptr_;

    // Append extra assignments to the initial state
    result.initial_state_ = initial_state_;
    result.initial_state_.insert(result.initial_state_.end(),
                                  extra_assignments.begin(),
                                  extra_assignments.end());

    return result;
}

std::string Problem::to_string() const {
    std::ostringstream oss;
    oss << "Problem: " << problem_name_ << " (Domain: " << domain_name_ << ")\n";
    
    oss << "\nObjects (" << objects_.size() << "):";
    for (const auto& obj : objects_) {
        oss << "\n  " << obj.to_string();
    }
    
    oss << "\n\nFluents (" << fluents_.size() << "):";
    for (const auto& fluent : fluents_) {
        oss << "\n  " << fluent.to_string();
    }
    
    oss << "\n\nActions (" << actions_.size() << "):";
    for (const auto& action : actions_) {
        oss << "\n  " << action.to_string();
    }

    oss << "\n\nGrounded Fluents (" << grounded_fluents_.size() << "):";
    for (const auto& eid : grounded_fluents_) {
        oss << "\n  " << pool().to_string(eid);
    }
    
    oss << "\n\nInitial State (" << initial_state_.size() << " assignments):";
    for (const auto& assignment : initial_state_) {
        oss << "\n  " << assignment.to_string();
    }
    
    oss << "\n\nGoals (" << goals_.size() << "):";
    for (const auto& goal : goals_) {
        oss << "\n  " << goal.to_string();
    }
    
    return oss.str();
}

void Problem::build_object_mappings() {
    object_name_to_index_.clear();
    for (size_t i = 0; i < objects_.size(); ++i) {
        object_name_to_index_[objects_[i].name()] = i;
    }
}

void Problem::build_fluent_mappings() {
    fluent_name_to_index_.clear();
    for (size_t i = 0; i < fluents_.size(); ++i) {
        fluent_name_to_index_[fluents_[i].name()] = i;
        fluents_[i].set_id(static_cast<int>(i));
    }
}

void Problem::build_action_mappings() {
    action_name_to_index_.clear();
    for (size_t i = 0; i < actions_.size(); ++i) {
        action_name_to_index_[actions_[i].name()] = i;
    }
}

void Problem::build_grounded_fluent_mappings() {
    grounded_fluent_to_index_.clear();
    for (size_t i = 0; i < grounded_fluents_.size(); ++i) {
        grounded_fluent_to_index_[grounded_fluents_[i]] = i;
    }
}

void Problem::load_types(const pb::RepeatedTypeDeclaration& pb_types) {
    types_->clear();
    type_name_to_ptr_.clear();

    // ensure basic types are present
    types_->emplace_back("up:bool");
    types_->emplace_back("up:int");
    types_->emplace_back("up:real");

    for (const auto& pb_type : pb_types) {
        types_->emplace_back(pb_type.type_name());
        types_->back().set_parent_name(pb_type.parent_type());
    }

    // Build the name to pointer mapping
    for (auto& type : *types_) {
        type_name_to_ptr_[type.name()] = &type;
    }
}

void Problem::resolve_type_hierarchy() {
    for (auto& type : *types_) {
        if (!type.parent_name().empty()) {
            auto it = type_name_to_ptr_.find(type.parent_name());
            if (it != type_name_to_ptr_.end()) {
                type.set_parent(it->second);
            }
        }
    }
}

void Problem::load_objects(const pb::RepeatedObjectDeclaration& pb_objects) {
    objects_.clear();
    for (const auto& pb_obj : pb_objects) {
        const Type* type = find_type(pb_obj.type());
        objects_.emplace_back(pb_obj.name(), type);
    }
    build_object_mappings();
}

void Problem::load_fluents(const pb::RepeatedFluent& pb_fluents) {
    fluents_.clear();
    for (const auto& pb_fluent : pb_fluents) {
        std::vector<Parameter> params;
        // Collect parameters for the fluent
        for (const auto& pb_param : pb_fluent.parameters()) {
            const Type* param_type = find_type(pb_param.type());
            params.emplace_back(pb_param.name(), param_type);
        }

        // Find the value type for the fluent
        const Type* value_type = find_type(pb_fluent.value_type());
        if (!value_type) {
            std::cerr << "ERROR: Could not find type '" << pb_fluent.value_type() 
                << "' for fluent '" << pb_fluent.name() << "'" << std::endl;
        }
        fluents_.emplace_back(pb_fluent.name(), value_type, params);
    }
    build_fluent_mappings();
}

void Problem::load_actions(const pb::RepeatedAction& pb_actions) {
    actions_.clear();
    for (const auto& pb_action : pb_actions) {
        std::vector<Parameter> params;
        for (const auto& pb_param : pb_action.parameters()) {
            const Type* param_type = find_type(pb_param.type());
            params.emplace_back(pb_param.name(), param_type);
        }
        actions_.emplace_back(pb_action, params, this);
        // Set the ID to match the vector index
        actions_.back().set_id(actions_.size() - 1);
    }
    build_action_mappings();
}

int Problem::find_grounded_fluent_index(ExprID eid) const {
    if (!eid.valid()) return -1;
    auto it = grounded_fluent_to_index_.find(eid);
    return (it != grounded_fluent_to_index_.end()) ? static_cast<int>(it->second) : -1;
}

} // namespace rantanplan
