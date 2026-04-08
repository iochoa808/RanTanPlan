#include "propagator_strategy.hpp"
#include "../../config/config.hpp"
#include "../../encoders/z3_variable_factory.hpp"

namespace rantanplan {

PropagatorStrategy::PropagatorStrategy(z3::solver& solver,
                                       const Problem& problem,
                                       BaseEncoder& encoder)
    : z3::user_propagator_base(&solver) {
    register_fixed();
    register_final();

    solver.set("smt.up.persist_clauses", Config::instance().global.persist_clauses);

    shared_.problem = &problem;
    shared_.variable_factory = &encoder.get_variable_factory();
    shared_.parallelism = encoder.get_parallelism_strategy();
    shared_.interference = shared_.parallelism->get_interference_analyzer();
    shared_.encoder = &encoder;
    shared_.solver = &solver;
}

void PropagatorStrategy::add_module(std::unique_ptr<PropagatorModule> m) {
    if (m->wants_decide_callback()) register_decide_callback();
    m->initialize(shared_, *this);
    modules_.push_back(std::move(m));
}

// ---------------------------------------------------------------------------
// Z3 API delegation
// ---------------------------------------------------------------------------

void PropagatorStrategy::module_conflict(const z3::expr_vector& fixed) {
    conflict(fixed);
    conflict_raised_ = true;
}

void PropagatorStrategy::module_propagate(const z3::expr_vector& fixed,
                                          const z3::expr& consequence) {
    propagate(fixed, consequence);
}

void PropagatorStrategy::module_add(const z3::expr& e) {
    if (registered_ids_.insert(e.id()).second)
        add(e);
}

void PropagatorStrategy::register_decide_callback() {
    if (!decide_registered_) {
        register_decide();
        decide_registered_ = true;
    }
}

// ---------------------------------------------------------------------------
// Shared action trail management
// ---------------------------------------------------------------------------

bool PropagatorStrategy::update_action_trail(const z3::expr& ast,
                                             const z3::expr& value) {
    if (!value.is_true()) return false;
    auto action_info = shared_.variable_factory->get_action_from_variable(ast);
    if (!action_info) return false;

    const Action& action = action_info->first;
    int timestep = action_info->second;
    shared_.action_trail.push_back({action.id(), timestep});
    shared_.active_actions_per_timestep[timestep].insert(action.id());
    return true;
}

// ---------------------------------------------------------------------------
// Z3 callbacks
// ---------------------------------------------------------------------------

void PropagatorStrategy::push() {
    shared_.action_decision_levels.push_back(shared_.action_trail.size());
    for (auto& m : modules_) m->on_push();
}

void PropagatorStrategy::pop(unsigned num_scopes) {
    // Restore shared action trail
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (!shared_.action_decision_levels.empty()) {
            size_t level_start = shared_.action_decision_levels.back();
            shared_.action_decision_levels.pop_back();
            while (shared_.action_trail.size() > level_start) {
                const auto& [action_node_id, timestep] = shared_.action_trail.back();
                shared_.active_actions_per_timestep[timestep].erase(action_node_id);
                shared_.action_trail.pop_back();
            }
        }
    }
    // Dispatch to modules (each undoes its private trail)
    for (auto& m : modules_) m->on_pop(num_scopes);
}

void PropagatorStrategy::fixed(const z3::expr& ast, const z3::expr& value) {
    conflict_raised_ = false;
    update_action_trail(ast, value);
    for (auto& m : modules_) {
        m->on_fixed(ast, value);
        if (conflict_raised_) break;
    }
}

void PropagatorStrategy::decide(const z3::expr& val, unsigned bit, bool is_pos) {
    for (auto& m : modules_) m->on_decide(val, bit, is_pos);
}

void PropagatorStrategy::final() {
    conflict_raised_ = false;
    for (auto& m : modules_) {
        m->on_final();
        if (conflict_raised_) break;
    }
}

z3::user_propagator_base* PropagatorStrategy::fresh(z3::context& /*ctx*/) {
    return nullptr;
}

// ---------------------------------------------------------------------------
// Variable registration
// ---------------------------------------------------------------------------

void PropagatorStrategy::register_timestep_variables(int timestep) {
    // Register action variables in shared state (once per timestep)
    if (timestep > 0 && !shared_.registered_action_vars.contains(timestep - 1)) {
        auto vars = shared_.variable_factory->get_all_action_variables(timestep - 1);
        if (!vars.empty()) {
            shared_.registered_action_vars[timestep - 1] = std::move(vars);
            for (const auto& v : shared_.registered_action_vars[timestep - 1]) {
                registered_ids_.insert(v->id());
                add(*v);
            }
        }
    }

    // Let modules register their own variables
    for (auto& m : modules_) m->register_timestep_variables(timestep);
}

// ---------------------------------------------------------------------------
// Delegated interface
// ---------------------------------------------------------------------------

void PropagatorStrategy::cleanup() {
    for (auto& m : modules_) m->cleanup();
}

std::string PropagatorStrategy::get_name() const {
    std::string name = "Propagator[";
    for (size_t i = 0; i < modules_.size(); ++i) {
        if (i > 0) name += ", ";
        name += modules_[i]->get_name();
    }
    name += "]";
    return name;
}

bool PropagatorStrategy::manages_parallelism_constraints() const {
    for (const auto& m : modules_)
        if (m->manages_parallelism_constraints()) return true;
    return false;
}

void PropagatorStrategy::on_action_activated(int action_id, int max_timestep) {
    for (auto& m : modules_)
        m->on_action_activated(action_id, max_timestep);
}

} // namespace rantanplan
