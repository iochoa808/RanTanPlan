#include "numeric_relaxed_planning_graph.hpp"
#include "../config/config.hpp"
#include "../util/memory_tracker.hpp"
#include "../util/scoped_timer.hpp"
#include "../util/logger.hpp"
#include "../util/stats.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <limits>

namespace rantanplan {

// ---------------------------------------------------------------------------
// Static helper: evaluate a (possibly lifted) expression to an interval
// using only constant fluent schema ranges. Used by precompute_freezes().
// Mirrors evaluate_constant_expr_range from numeric_bounds_index.cpp.
// ---------------------------------------------------------------------------

static Interval evaluate_constant_expr_range(
        ExprID eid,
        const ExprPool& pool,
        const Problem& problem,
        const std::unordered_map<int, Interval>& constant_ranges) {
    if (!eid.valid()) return Interval::unbounded();

    if (pool.is_constant(eid)) {
        if (pool.payload_is_double(eid))
            return Interval(pool.payload_double(eid));
        if (pool.payload_is_int(eid))
            return Interval(static_cast<double>(pool.payload_int(eid)));
        return Interval::unbounded();
    }

    if (pool.is_state_variable(eid)) {
        ExprID head = pool.head_symbol_id(eid);
        if (!pool.is_fluent_symbol(head)) return Interval::unbounded();
        const std::string& fname = pool.payload_string(head);
        const Fluent* fluent = problem.find_fluent(fname);
        if (!fluent || fluent->is_predicate()) return Interval::unbounded();
        auto it = constant_ranges.find(fluent->id());
        if (it != constant_ranges.end()) return it->second;
        return Interval::unbounded();  // Non-constant fluent.
    }

    if (pool.is_function_application(eid)) {
        ExprOperator op = pool.op(eid);
        size_t nargs = pool.argument_count(eid);

        if (nargs == 2) {
            Interval lhs = evaluate_constant_expr_range(
                pool.argument(eid, 0), pool, problem, constant_ranges);
            Interval rhs = evaluate_constant_expr_range(
                pool.argument(eid, 1), pool, problem, constant_ranges);
            switch (op) {
                case ExprOperator::PLUS:     return lhs + rhs;
                case ExprOperator::MINUS:    return lhs - rhs;
                case ExprOperator::MULTIPLY: return lhs * rhs;
                case ExprOperator::DIVIDE:   return lhs / rhs;
                default: break;
            }
        }
        if (nargs == 1 && op == ExprOperator::MINUS) {
            Interval arg = evaluate_constant_expr_range(
                pool.argument(eid, 0), pool, problem, constant_ranges);
            return Interval(0.0) - arg;
        }
    }

    return Interval::unbounded();
}

// ============================================================================
// STATIC MEMBER INITIALIZATION
// ============================================================================

const std::vector<const Action*> NumericRelaxedPlanningGraph::empty_action_vector_;

// ============================================================================
// PUBLIC TYPES - LayerState
// ============================================================================

bool NumericRelaxedPlanningGraph::LayerState::operator==(const LayerState& other) const {
    // Check Boolean reachability equality
    if (boolean_reachability.size() != other.boolean_reachability.size()) {
        return false;
    }
    for (const auto& [fluent_id, reach] : boolean_reachability) {
        auto it = other.boolean_reachability.find(fluent_id);
        if (it == other.boolean_reachability.end() || it->second != reach) {
            return false;
        }
    }

    // Check numeric bounds equality (with tolerance)
    if (numeric_bounds.size() != other.numeric_bounds.size()) {
        return false;
    }
    for (const auto& [fluent_id, bounds] : numeric_bounds) {
        auto it = other.numeric_bounds.find(fluent_id);
        if (it == other.numeric_bounds.end() || it->second != bounds) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

NumericRelaxedPlanningGraph::NumericRelaxedPlanningGraph(const Problem& problem)
    : problem_(problem),
      ctx_(),
      variable_factory_(ctx_),
      grounded_visitor_(ctx_, &problem, &variable_factory_),
      max_layers_(Config::instance().planner.max_steps),  // Read from config
      stop_when_all_reachable_(true),
      build_time_ms_(0.0),
      total_smt_queries_(0),
      total_applicability_checks_(0),
      total_interval_checks_(0) {

    // Build EPC index for efficient effect lookup
    build_epc_index();

    // Classify fluents into Boolean vs numeric
    classify_fluents();

    // Build ground fluent -> schema mapping and run freeze analysis
    build_fluent_schema_map();
    precompute_freezes();

    Logger::instance().debug("NumericRelaxedPlanningGraph initialized:");
    Logger::instance().debug("  - Total fluents: " + std::to_string(problem_.grounded_fluents().size()));
    Logger::instance().debug("  - Boolean fluents: " + std::to_string(boolean_fluent_ids_.size()));
    Logger::instance().debug("  - Numeric fluents: " + std::to_string(numeric_fluent_ids_.size()));
    Logger::instance().debug("  - Total actions: " + std::to_string(problem_.actions().size()));
    Logger::instance().debug("  - Max layers: " + std::to_string(max_layers_));
}

// ============================================================================
// MAIN INTERFACE
// ============================================================================

// ============================================================================
// PRIVATE METHODS - Utilities
// ============================================================================

std::string NumericRelaxedPlanningGraph::reachability_to_string(BooleanReachability r) const {
    switch (r) {
        case BooleanReachability::FALSE_ONLY:
            return "FALSE_ONLY";
        case BooleanReachability::TRUE_ONLY:
            return "TRUE_ONLY";
        case BooleanReachability::BOTH:
            return "BOTH";
        default:
            return "UNKNOWN";
    }
}

// ============================================================================
// PRIVATE METHODS - Expression Analysis
// ============================================================================

bool NumericRelaxedPlanningGraph::is_boolean_expression(ExprID eid) const {
    return problem_.is_bool_type(eid);
}

bool NumericRelaxedPlanningGraph::is_numeric_expression(ExprID eid) const {
    return problem_.is_numeric_type(eid);
}

int NumericRelaxedPlanningGraph::find_grounded_fluent_id(ExprID fluent_eid) const {
    // Use Problem's find_grounded_fluent_index for O(1) lookup
    int index = problem_.find_grounded_fluent_index(fluent_eid);
    if (index < 0) {
        std::cerr << "Error: Fluent not found in grounded fluents: "
                  << problem_.pool().to_string(fluent_eid) << std::endl;
        return -1;
    }
    return index;
}

// ============================================================================
// PRIVATE METHODS - Initialization
// ============================================================================

void NumericRelaxedPlanningGraph::build_epc_index() {
    // BORROWED PATTERN FROM GroundedEncoder::build_epc_index()
    // Maps each fluent to the actions/effects that can modify it

    epc_index_.clear();

    // Initialize index with all grounded fluents (empty effect lists)
    // This ensures ALL fluents get frame axioms, including those from
    // initial state, action effects, preconditions, and goals
    for (ExprID eid : problem_.grounded_fluents()) {
        epc_index_[eid] = std::vector<std::pair<const Action*, const EffectExpression*>>();
    }

    // Add action effects to the fluents that can be modified
    for (const Action& action : problem_.actions()) {
        for (const Effect& effect : action.effects()) {
            const EffectExpression& eff_expr = effect.effect_expression();
            ExprID fluent_eid = eff_expr.fluent_id();
            epc_index_[fluent_eid].emplace_back(&action, &eff_expr);
        }
    }

    Logger::instance().debug("EPC index built: " + std::to_string(epc_index_.size()) + " fluents indexed");
    size_t total_effects = 0;
    for (const auto& [eid, effects] : epc_index_) {
        total_effects += effects.size();
    }
    Logger::instance().debug("  Total effect entries: " + std::to_string(total_effects));
}

void NumericRelaxedPlanningGraph::classify_fluents() {
    // Classify each grounded fluent as Boolean or numeric based on type
    boolean_fluent_ids_.clear();
    numeric_fluent_ids_.clear();

    int fluent_id = 0;
    for (ExprID eid : problem_.grounded_fluents()) {
        if (problem_.is_bool_type(eid)) {
            boolean_fluent_ids_.insert(fluent_id);
        } else if (problem_.is_numeric_type(eid)) {
            numeric_fluent_ids_.insert(fluent_id);
        } else {
            // Object-typed fluents are treated as Boolean (equality checks)
            boolean_fluent_ids_.insert(fluent_id);
        }
        fluent_id++;
    }
}

void NumericRelaxedPlanningGraph::initialize_layer_0() {
    // Initialize layer 0 from fully-defined initial state
    // PATTERN BORROWED FROM GroundedEncoder::encode_initial_state()

    LayerState initial_layer;

    const auto& pool = problem_.pool();

    // Process each assignment in the initial state
    for (const auto& assignment : problem_.initial_state()) {
        ExprID fluent_eid = assignment.fluent_id();
        int fluent_id = find_grounded_fluent_id(fluent_eid);
        if (fluent_id < 0) {
            continue;  // Skip if not found
        }

        ExprID val_eid = assignment.value_id();

        if (boolean_fluent_ids_.contains(fluent_id)) {
            // Boolean fluent: check if value is true or false
            if (pool.is_constant(val_eid) && pool.payload_is_bool(val_eid)) {
                bool is_true = pool.payload_bool(val_eid);
                initial_layer.boolean_reachability[fluent_id] =
                    is_true ? BooleanReachability::TRUE_ONLY
                            : BooleanReachability::FALSE_ONLY;
            } else {
                // Object-typed fluents end up here: their initial values are object
                // constants (not true/false), so this is expected. We treat them as
                // FALSE_ONLY for reachability purposes (conservative over-approximation).
                initial_layer.boolean_reachability[fluent_id] = BooleanReachability::FALSE_ONLY;
            }
        } else if (numeric_fluent_ids_.contains(fluent_id)) {
            // Numeric fluent: extract value and create point bounds
            if (pool.is_constant(val_eid)) {
                double numeric_value = 0.0;
                if (pool.payload_is_int(val_eid)) {
                    numeric_value = static_cast<double>(pool.payload_int(val_eid));
                } else if (pool.payload_is_double(val_eid)) {
                    numeric_value = pool.payload_double(val_eid);
                } else {
                    std::cerr << "Warning: Numeric fluent has non-numeric value in initial state: "
                              << pool.to_string(fluent_eid) << std::endl;
                }
                initial_layer.numeric_bounds[fluent_id] = NumericBounds(numeric_value);
            } else {
                std::cerr << "Warning: Numeric fluent has non-constant value in initial state: "
                          << pool.to_string(fluent_eid) << std::endl;
                initial_layer.numeric_bounds[fluent_id] = NumericBounds(0.0);
            }
        }
    }

    // Store layer 0
    layer_states_.push_back(std::move(initial_layer));
    action_layers_.push_back(std::vector<const Action*>());  // No actions at layer 0

    Logger::instance().debug("Layer 0 initialized:");
    Logger::instance().debug("  - Boolean fluents: " + std::to_string(layer_states_[0].boolean_reachability.size()));
    Logger::instance().debug("  - Numeric fluents: " + std::to_string(layer_states_[0].numeric_bounds.size()));
}

// ============================================================================
// MAIN INTERFACE
// ============================================================================

bool NumericRelaxedPlanningGraph::build() {
    auto& config = Config::instance();
    ScopedTimer timer("rpg.numeric.build_time_ms");
    double start_memory = MemoryTracker::instance().get_current_memory_mb();

    // Initialize layer 0 from initial state
    initialize_layer_0();

    Logger::instance().component(VerbosityLevel::VERBOSE, "RPG.Numeric", {
        {"status", "starting layer expansion"}
    });

    // Debug: Print initial layer (layer 0)
    print_layer_summary(0);

    // Layer-by-layer construction until fixpoint or max_layers
    for (int layer = 0; layer < max_layers_; ++layer) {
        // Compute applicable actions using SMT-based checking
        std::vector<const Action*> applicable_actions = compute_applicable_actions(layer);

        // If no actions are applicable and we've already built at least one layer, we're done
        if (applicable_actions.empty() && layer > 0) {
            Logger::instance().verbose("Fixpoint reached at layer " + std::to_string(layer));
            break;
        }

        // Create next layer state
        layer_states_.push_back(LayerState());

        // Store applicable actions for this layer
        action_layers_.push_back(applicable_actions);

        // Propagate Boolean effects
        propagate_boolean_effects(applicable_actions, layer, layer + 1);

        // Compute numeric bounds
        compute_numeric_bounds(applicable_actions, layer, layer + 1);

        // Debug: Print layer summary and delta (only in debug mode)
        if (Config::instance().global.verbosity >= VerbosityLevel::DEBUG) {
            print_layer_delta(layer, layer + 1);
        }

        // Compact layer output
        const auto& new_layer = layer_states_[layer + 1];

        // Count Boolean fluent states
        int false_only_count = 0;
        int true_only_count = 0;
        int both_count = 0;
        for (const auto& [fluent_id, reach] : new_layer.boolean_reachability) {
            if (reach == BooleanReachability::FALSE_ONLY) false_only_count++;
            else if (reach == BooleanReachability::TRUE_ONLY) true_only_count++;
            else if (reach == BooleanReachability::BOTH) both_count++;
        }

        std::ostringstream component_name, bool_states;
        component_name << "RPG.Numeric L" << (layer + 1);
        bool_states << "F:" << false_only_count << "/T:" << true_only_count << "/B:" << both_count;

        Logger::instance().component(VerbosityLevel::VERBOSE, component_name.str(), {
            {"actions", std::to_string(applicable_actions.size())},
            {"bool", std::to_string(new_layer.boolean_reachability.size()) + " (" + bool_states.str() + ")"},
            {"num", std::to_string(new_layer.numeric_bounds.size())}
        });

        // Check for fixpoint
        if (is_fixpoint_reached()) {
            Logger::instance().verbose("Fixpoint reached - terminating layer expansion");
            break;
        }

        // Early termination if goals are achievable (if enabled)
        if (config.rpg.early_goal_termination && are_goals_achievable()) {
            Logger::instance().verbose("Goals achievable - early termination at layer " + std::to_string(layer + 1));
            break;
        }

        // Early termination if all actions reachable and goals achieved (if enabled)
        if (stop_when_all_reachable_ &&
            are_all_actions_reachable() &&
            are_goals_achievable()) {
            Logger::instance().verbose("All " + std::to_string(action_layers_.back().size()) + 
                                      "/" + std::to_string(problem_.actions().size()) +
                                      " actions reachable and goals achievable - early termination at layer " + 
                                      std::to_string(layer + 1));
            break;
        }
    }

    build_time_ms_ = timer.elapsed_ms();
    double end_memory = MemoryTracker::instance().get_current_memory_mb();
    double memory_used = end_memory - start_memory;

    bool goals_reachable = are_goals_achievable();

    // Count reachable actions
    int total_actions = problem_.actions().size();
    std::unordered_set<const Action*> reachable_action_set;
    for (const auto& layer_actions : action_layers_) {
        for (const Action* action : layer_actions) {
            reachable_action_set.insert(action);
        }
    }
    int reachable_actions = reachable_action_set.size();
    int removed_actions = total_actions - reachable_actions;

    // Record to Stats
    Stats::instance().set("rpg.numeric.total_layers", layer_states_.size());
    Stats::instance().set("rpg.numeric.smt_queries", total_smt_queries_);
    Stats::instance().set("rpg.numeric.applicability_checks", total_applicability_checks_);
    Stats::instance().set("rpg.numeric.interval_checks", total_interval_checks_);
    Stats::instance().set("rpg.numeric.total_actions", total_actions);
    Stats::instance().set("rpg.numeric.reachable_actions", reachable_actions);
    Stats::instance().set("rpg.numeric.removed_actions", removed_actions);
    Stats::instance().set("rpg.numeric.memory_mb", memory_used);
    Stats::instance().set("rpg.numeric.goals_reachable", goals_reachable ? 1.0 : 0.0);

    // Structured visual output
    std::ostringstream actions_str;
    actions_str << total_actions << " (" << reachable_actions << " reachable, " << removed_actions << " removed)";

    std::vector<std::pair<std::string, std::string>> log_fields = {
        {"time", std::to_string(static_cast<int>(build_time_ms_)) + "ms"},
        {"layers", std::to_string(layer_states_.size())},
        {"actions", actions_str.str()},
    };
    if (goals_reachable) {
        int lower_bound = get_minimum_steps_lower_bound();
        log_fields.push_back({"lower bound", std::to_string(lower_bound)});
    }
    if (total_smt_queries_ > 0) {
        log_fields.push_back({"SMT queries", std::to_string(total_smt_queries_)});
    }
    log_fields.push_back({"mem", std::to_string(static_cast<int>(memory_used)) + "MB"});
    log_fields.push_back({"goals", goals_reachable ? "REACHABLE" : "UNREACHABLE"});

    Logger::instance().component(VerbosityLevel::INFO, "RPG.Numeric", log_fields);

    return goals_reachable;
}

// ============================================================================
// PRIVATE METHODS - Boolean Effect Propagation
// ============================================================================

void NumericRelaxedPlanningGraph::propagate_boolean_effects(
    const std::vector<const Action*>& applicable_actions,
    int prev_layer,
    int next_layer) {

    auto& config = Config::instance();
    const auto& prev_state = layer_states_[prev_layer];
    auto& next_state = layer_states_[next_layer];

    // Start by copying previous layer's Boolean reachability (frame axiom)
    next_state.boolean_reachability = prev_state.boolean_reachability;

    // For each Boolean fluent, check if any applicable action has an effect on it
    for (int fluent_id : boolean_fluent_ids_) {
        // Get current reachability state
        BooleanReachability current_state = BooleanReachability::FALSE_ONLY;
        auto it = prev_state.boolean_reachability.find(fluent_id);
        if (it != prev_state.boolean_reachability.end()) {
            current_state = it->second;
        }

        // Get fluent ExprID for EPC lookup
        ExprID fluent_eid = problem_.grounded_fluent(fluent_id);

        // Find all effects from applicable actions that modify this fluent
        auto epc_it = epc_index_.find(fluent_eid);
        if (epc_it == epc_index_.end()) {
            continue;  // No effects for this fluent
        }

        // Apply each effect from applicable actions
        BooleanReachability new_state = current_state;
        for (const auto& [action, effect_expr] : epc_it->second) {
            // Check if this action is in the applicable actions list
            bool is_applicable = false;
            for (const Action* app_action : applicable_actions) {
                if (app_action == action) {
                    is_applicable = true;
                    break;
                }
            }

            if (is_applicable) {
                apply_boolean_effect(*effect_expr, current_state, new_state, next_layer);
                // Once we reach BOTH, we can stop (absorbing state)
                if (new_state == BooleanReachability::BOTH) {
                    break;
                }
            }
        }

        // Update next layer state
        next_state.boolean_reachability[fluent_id] = new_state;
    }

    Logger::instance().debug("  Boolean effects propagated for " + std::to_string(boolean_fluent_ids_.size()) + " fluents");
}

void NumericRelaxedPlanningGraph::apply_boolean_effect(
    const EffectExpression& effect,
    BooleanReachability current_state,
    BooleanReachability& next_state,
    int layer) const {

    // Determine if this is a positive effect (sets to true) or negative (sets to false)
    const auto& pool = problem_.pool();
    ExprID val_eid = effect.value_id();

    bool is_positive_effect = false;
    bool is_negative_effect = false;

    // Check if the effect value is a Boolean constant
    if (pool.is_constant(val_eid) && pool.payload_is_bool(val_eid)) {
        bool effect_value = pool.payload_bool(val_eid);
        is_positive_effect = effect_value;   // Sets to true
        is_negative_effect = !effect_value;  // Sets to false
    } else {
        // Non-constant Boolean expression - conservatively assume it could be either
        // This is a simplification; proper handling would require SMT analysis
        is_positive_effect = true;
        is_negative_effect = true;
    }

    // Apply ADL DELETE-RELAXATION semantics with 3-valued logic
    // State transitions:
    // - FALSE_ONLY + positive effect → BOTH (can now be true OR stay false)
    // - FALSE_ONLY + negative effect → FALSE_ONLY (no change, already can only be false)
    // - TRUE_ONLY + positive effect → TRUE_ONLY (no change, already can only be true)
    // - TRUE_ONLY + negative effect → BOTH (can now be false OR stay true)
    // - BOTH + any effect → BOTH (absorbing state)

    if (current_state == BooleanReachability::BOTH) {
        // Absorbing state - no change
        next_state = BooleanReachability::BOTH;
    } else if (current_state == BooleanReachability::FALSE_ONLY) {
        if (is_positive_effect) {
            // Can now make it true OR leave it false → BOTH
            next_state = BooleanReachability::BOTH;
        } else {
            // Negative effect on FALSE_ONLY → no change
            next_state = BooleanReachability::FALSE_ONLY;
        }
    } else if (current_state == BooleanReachability::TRUE_ONLY) {
        if (is_negative_effect) {
            // Can now make it false OR leave it true → BOTH
            next_state = BooleanReachability::BOTH;
        } else {
            // Positive effect on TRUE_ONLY → no change
            next_state = BooleanReachability::TRUE_ONLY;
        }
    }
}

// ============================================================================
// PRIVATE METHODS - Numeric Bounds Computation
// ============================================================================

void NumericRelaxedPlanningGraph::compute_numeric_bounds(
    const std::vector<const Action*>& applicable_actions,
    int prev_layer,
    int next_layer) {

    const auto& prev_state = layer_states_[prev_layer];
    auto& next_state = layer_states_[next_layer];

    // Start by copying previous layer's numeric bounds (frame axiom - persistence)
    next_state.numeric_bounds = prev_state.numeric_bounds;

    // For each numeric fluent, compute new bounds via interval arithmetic
    for (int fluent_id : numeric_fluent_ids_) {
        NumericBounds new_bounds = compute_single_variable_bounds_interval(
            fluent_id, applicable_actions, prev_layer);

        // Get previous bounds for widening comparison
        NumericBounds prev_bounds(0.0);
        auto it = prev_state.numeric_bounds.find(fluent_id);
        if (it != prev_state.numeric_bounds.end()) {
            prev_bounds = it->second;
        }

        // Apply directional widening (modifies new_bounds in place)
        apply_widening(fluent_id, new_bounds, prev_bounds);

        next_state.numeric_bounds[fluent_id] = new_bounds;
    }

    Logger::instance().debug("  Numeric bounds computed for " + std::to_string(numeric_fluent_ids_.size()) + " fluents (interval arithmetic)");
}

NumericRelaxedPlanningGraph::NumericBounds NumericRelaxedPlanningGraph::compute_single_variable_bounds_interval(
    int fluent_id,
    const std::vector<const Action*>& applicable_actions,
    int prev_layer) const {

    // Get current bounds (persistence branch)
    NumericBounds current_bounds(0.0);
    auto it = layer_states_[prev_layer].numeric_bounds.find(fluent_id);
    if (it != layer_states_[prev_layer].numeric_bounds.end()) {
        current_bounds = it->second;
    }

    // Get all effects for this fluent from applicable actions
    std::vector<const EffectExpression*> effects = get_effects_for_fluent(fluent_id, applicable_actions);

    // If no effects, bounds persist (frame axiom)
    if (effects.empty()) {
        return current_bounds;
    }

    // Start with persistence: current bounds carry forward
    Interval result(current_bounds.lower, current_bounds.upper);
    Interval current_iv(current_bounds.lower, current_bounds.upper);

    // Evaluate each effect branch and take convex union
    for (const EffectExpression* effect_expr : effects) {
        Interval value_iv = evaluate_interval(effect_expr->value_id(), prev_layer);

        Interval branch = Interval::unbounded();  // safe fallback
        switch (effect_expr->kind()) {
            case EffectExpression::Kind::ASSIGN:
                branch = value_iv;
                break;
            case EffectExpression::Kind::INCREASE:
                branch = current_iv + value_iv;
                break;
            case EffectExpression::Kind::DECREASE:
                branch = current_iv - value_iv;
                break;
        }

        result = result.convex_union(branch);
    }

    return NumericBounds(result.lower, result.upper);
}

Interval NumericRelaxedPlanningGraph::evaluate_interval(ExprID eid, int layer) const {
    const auto& pool = problem_.pool();

    if (!eid.valid()) return Interval::unbounded();

    // Constants -> point interval
    if (pool.is_constant(eid)) {
        if (pool.payload_is_double(eid))
            return Interval(pool.payload_double(eid));
        if (pool.payload_is_int(eid))
            return Interval(static_cast<double>(pool.payload_int(eid)));
        // Boolean or string constant — not numeric
        return Interval::unbounded();
    }

    // State variables -> lookup in layer's numeric_bounds
    if (pool.is_state_variable(eid)) {
        int fluent_id = find_grounded_fluent_id(eid);
        if (fluent_id >= 0 && numeric_fluent_ids_.count(fluent_id)) {
            auto it = layer_states_[layer].numeric_bounds.find(fluent_id);
            if (it != layer_states_[layer].numeric_bounds.end()) {
                return Interval(it->second.lower, it->second.upper);
            }
        }
        // Uninitialized numeric fluent defaults to [0, 0]
        return Interval(0.0);
    }

    // Arithmetic operations
    if (pool.is_function_application(eid)) {
        ExprOperator op = pool.op(eid);
        size_t nargs = pool.argument_count(eid);

        if (nargs == 2) {
            Interval lhs = evaluate_interval(pool.argument(eid, 0), layer);
            Interval rhs = evaluate_interval(pool.argument(eid, 1), layer);
            switch (op) {
                case ExprOperator::PLUS:     return lhs + rhs;
                case ExprOperator::MINUS:    return lhs - rhs;
                case ExprOperator::MULTIPLY: return lhs * rhs;
                case ExprOperator::DIVIDE:   return lhs / rhs;
                default: break;
            }
        }
        // Unary minus
        if (nargs == 1 && op == ExprOperator::MINUS) {
            Interval arg = evaluate_interval(pool.argument(eid, 0), layer);
            return Interval(0.0) - arg;
        }
    }

    return Interval::unbounded();
}

void NumericRelaxedPlanningGraph::apply_widening(
    int fluent_id, NumericBounds& new_bounds, const NumericBounds& prev_bounds) {

    // Track which side(s) actually moved
    if (new_bounds.lower < prev_bounds.lower) {
        lower_expansion_count_[fluent_id]++;
    }
    if (new_bounds.upper > prev_bounds.upper) {
        upper_expansion_count_[fluent_id]++;
    }

    // Look up schema ID for freeze check
    int schema_id = -1;
    auto schema_it = fluent_schema_map_.find(fluent_id);
    if (schema_it != fluent_schema_map_.end()) {
        schema_id = schema_it->second;
    }

    // Widen lower if threshold exceeded and not frozen
    if (schema_id < 0 || !freeze_lower_.count(schema_id)) {
        auto count_it = lower_expansion_count_.find(fluent_id);
        if (count_it != lower_expansion_count_.end() &&
            count_it->second >= WIDENING_THRESHOLD &&
            !std::isinf(new_bounds.lower)) {
            new_bounds.lower = -std::numeric_limits<double>::infinity();
        }
    }

    // Widen upper if threshold exceeded and not frozen
    if (schema_id < 0 || !freeze_upper_.count(schema_id)) {
        auto count_it = upper_expansion_count_.find(fluent_id);
        if (count_it != upper_expansion_count_.end() &&
            count_it->second >= WIDENING_THRESHOLD &&
            !std::isinf(new_bounds.upper)) {
            new_bounds.upper = std::numeric_limits<double>::infinity();
        }
    }
}

void NumericRelaxedPlanningGraph::build_fluent_schema_map() {
    const auto& pool = problem_.pool();
    int fluent_id = 0;
    for (ExprID eid : problem_.grounded_fluents()) {
        if (numeric_fluent_ids_.count(fluent_id)) {
            // Extract schema ID from the ground state variable
            if (pool.is_state_variable(eid)) {
                ExprID head = pool.head_symbol_id(eid);
                if (pool.is_fluent_symbol(head)) {
                    const std::string& fname = pool.payload_string(head);
                    const Fluent* fluent = problem_.find_fluent(fname);
                    if (fluent && fluent->is_function()) {
                        fluent_schema_map_[fluent_id] = fluent->id();
                    }
                }
            }
        }
        fluent_id++;
    }
}

void NumericRelaxedPlanningGraph::precompute_freezes() {
    const auto& pool = problem_.pool();

    // Step 1: Identify which fluent schemas are modified by any effect
    std::unordered_set<int> modified_schemas;

    struct EffectInfo {
        EffectExpression::Kind kind;
        ExprID value_id;
    };
    std::unordered_map<int, std::vector<EffectInfo>> effects_per_schema;

    for (size_t si = 0; si < problem_.action_count(); ++si) {
        const Action& schema = problem_.action(si);
        for (const auto& eff : schema.effects()) {
            const auto& ee = eff.effect_expression();
            ExprID fluent_eid = ee.fluent_id();

            // Extract schema ID
            if (!pool.is_state_variable(fluent_eid)) continue;
            ExprID head = pool.head_symbol_id(fluent_eid);
            if (!pool.is_fluent_symbol(head)) continue;
            const std::string& fname = pool.payload_string(head);
            const Fluent* fluent = problem_.find_fluent(fname);
            if (!fluent || fluent->is_predicate()) continue;

            int fid = fluent->id();
            modified_schemas.insert(fid);
            effects_per_schema[fid].push_back({ee.kind(), ee.value_id()});
        }
    }

    // Step 2: Compute ranges for constant fluent schemas from initial state
    std::unordered_map<int, Interval> constant_ranges;
    for (const auto& fluent : problem_.fluents()) {
        if (fluent.is_function() && !modified_schemas.count(fluent.id())) {
            constant_ranges.emplace(fluent.id(), Interval(0.0));
        }
    }

    for (const auto& assignment : problem_.initial_state()) {
        ExprID fluent_eid = assignment.fluent_id();
        if (!pool.is_state_variable(fluent_eid)) continue;
        ExprID head = pool.head_symbol_id(fluent_eid);
        if (!pool.is_fluent_symbol(head)) continue;
        const std::string& fname = pool.payload_string(head);
        const Fluent* fluent = problem_.find_fluent(fname);
        if (!fluent || fluent->is_predicate()) continue;

        auto it = constant_ranges.find(fluent->id());
        if (it != constant_ranges.end()) {
            double val = 0.0;
            ExprID vid = assignment.value_id();
            if (pool.payload_is_double(vid))
                val = pool.payload_double(vid);
            else if (pool.payload_is_int(vid))
                val = static_cast<double>(pool.payload_int(vid));
            it->second = it->second.convex_union(Interval(val));
        }
    }

    // Step 3: For each non-constant fluent schema, classify effects
    // Use the same evaluate_constant_expr_range logic as the grounding layer
    for (const auto& [fid, effects] : effects_per_schema) {
        bool can_freeze_upper = true;
        bool can_freeze_lower = true;

        for (const auto& [kind, value_id] : effects) {
            Interval val_range = evaluate_constant_expr_range(value_id, pool, problem_, constant_ranges);

            if (kind == EffectExpression::Kind::ASSIGN) {
                if (val_range.is_unbounded()) {
                    can_freeze_upper = false;
                    can_freeze_lower = false;
                }
            } else if (kind == EffectExpression::Kind::INCREASE) {
                if (val_range.is_unbounded()) {
                    can_freeze_upper = false;
                    can_freeze_lower = false;
                } else {
                    if (val_range.upper > 0) can_freeze_upper = false;
                    if (val_range.lower < 0) can_freeze_lower = false;
                }
            } else if (kind == EffectExpression::Kind::DECREASE) {
                if (val_range.is_unbounded()) {
                    can_freeze_upper = false;
                    can_freeze_lower = false;
                } else {
                    if (val_range.upper > 0) can_freeze_lower = false;
                    if (val_range.lower < 0) can_freeze_upper = false;
                }
            }

            if (!can_freeze_upper && !can_freeze_lower) break;
        }

        if (can_freeze_upper) freeze_upper_.insert(fid);
        if (can_freeze_lower) freeze_lower_.insert(fid);
    }

    if (!freeze_upper_.empty() || !freeze_lower_.empty()) {
        Logger::instance().component(VerbosityLevel::VERBOSE, "RPG.Numeric", {
            {"freeze analysis", std::to_string(freeze_upper_.size()) + " upper, " +
                                std::to_string(freeze_lower_.size()) + " lower"}
        });
    }
}

// ============================================================================
// PRIVATE METHODS - Action Applicability
// ============================================================================

std::vector<const Action*> NumericRelaxedPlanningGraph::compute_applicable_actions(int layer) const {
    return compute_applicable_actions_individual(layer);
}

std::vector<const Action*> NumericRelaxedPlanningGraph::compute_applicable_actions_individual(int layer) const {
    auto& config = Config::instance();
    std::vector<const Action*> applicable_actions;

    // For each action, check if its precondition is satisfiable given the layer state
    bool use_interval = Config::instance().rpg.use_interval_checker;
    for (const Action& action : problem_.actions()) {
        bool applicable = use_interval
            ? is_action_applicable_interval(action, layer)
            : is_action_applicable_smt(action, layer);
        if (applicable) {
            applicable_actions.push_back(&action);
        }
    }

    Logger::instance().debug("  Found " + std::to_string(applicable_actions.size()) +
                            " applicable actions at layer " + std::to_string(layer) + " (individual queries)");

    return applicable_actions;
}

std::vector<const Action*> NumericRelaxedPlanningGraph::compute_applicable_actions_batch(int layer) const {
    auto& config = Config::instance();
    std::vector<const Action*> applicable_actions;

    // Create a fresh solver for this query
    z3::solver solver(ctx_);

    // Add layer state constraints
    add_layer_constraints(solver, layer);

    // Create action variables and add implication constraints
    std::unordered_map<std::string, const Action*> action_map;
    z3::expr_vector action_vars(ctx_);

    for (const Action& action : problem_.actions()) {
        std::string action_var_name = "action_" + action.name() + "_" + std::to_string(layer);
        z3::expr action_var = ctx_.bool_const(action_var_name.c_str());
        action_vars.push_back(action_var);
        action_map[action_var_name] = &action;

        // action_var → precondition (vacuously true if no precondition)
        if (action.has_precondition()) {
            z3::expr precondition = grounded_visitor_.convert_from_pool(action.precondition_id(), layer);
            solver.add(z3::implies(action_var, precondition));
        }
    }

    // Check satisfiability
    total_smt_queries_++;
    if (solver.check() == z3::sat) {
        z3::model model = solver.get_model();

        // Extract which actions are applicable
        for (unsigned i = 0; i < action_vars.size(); ++i) {
            z3::expr action_var = action_vars[i];
            z3::expr val = model.eval(action_var, true);

            if (val.is_true()) {
                std::string var_name = action_var.to_string();
                auto it = action_map.find(var_name);
                if (it != action_map.end()) {
                    applicable_actions.push_back(it->second);
                }
            }
        }
    }

    Logger::instance().debug("  Found " + std::to_string(applicable_actions.size()) +
                            " applicable actions at layer " + std::to_string(layer) + " (batch query)");

    return applicable_actions;
}

bool NumericRelaxedPlanningGraph::is_action_applicable_smt(const Action& action, int layer) const {
    // Create a fresh solver for this query
    z3::solver solver(ctx_);

    // Add layer state constraints
    add_layer_constraints(solver, layer);

    // Add action precondition (vacuously true if no precondition)
    if (action.has_precondition()) {
        z3::expr precondition = grounded_visitor_.convert_from_pool(action.precondition_id(), layer);
        solver.add(precondition);
    }

    // Check satisfiability
    total_smt_queries_++;
    total_applicability_checks_++;

    z3::check_result result = solver.check();
    return result == z3::sat;
}

// ============================================================================
// PRIVATE METHODS - SMT Constraint Building
// ============================================================================

void NumericRelaxedPlanningGraph::add_layer_constraints(z3::solver& solver, int layer) const {
    add_boolean_constraints(solver, layer);
    add_numeric_constraints(solver, layer);
}

void NumericRelaxedPlanningGraph::add_boolean_constraints(z3::solver& solver, int layer) const {
    const auto& layer_state = layer_states_[layer];

    // For each Boolean fluent, add constraints based on its reachability state
    for (const auto& [fluent_id, reach] : layer_state.boolean_reachability) {
        ExprID fluent_eid = problem_.grounded_fluent(fluent_id);
        z3::expr fluent_z3 = grounded_visitor_.convert_from_pool(fluent_eid, layer);

        // Add constraints based on reachability state
        switch (reach) {
            case BooleanReachability::FALSE_ONLY:
                // Fluent must be false
                solver.add(!fluent_z3);
                break;

            case BooleanReachability::TRUE_ONLY:
                // Fluent must be true
                solver.add(fluent_z3);
                break;

            case BooleanReachability::BOTH:
                // Fluent can be either true or false - no constraint needed
                break;
        }
    }
}

void NumericRelaxedPlanningGraph::add_numeric_constraints(z3::solver& solver, int layer) const {
    const auto& layer_state = layer_states_[layer];

    // For each numeric fluent, add bound constraints (skip infinite bounds)
    for (const auto& [fluent_id, bounds] : layer_state.numeric_bounds) {
        ExprID fluent_eid = problem_.grounded_fluent(fluent_id);
        z3::expr fluent_z3 = grounded_visitor_.convert_from_pool(fluent_eid, layer);

        if (!std::isinf(bounds.lower)) {
            solver.add(fluent_z3 >= ctx_.real_val(std::to_string(bounds.lower).c_str()));
        }
        if (!std::isinf(bounds.upper)) {
            solver.add(fluent_z3 <= ctx_.real_val(std::to_string(bounds.upper).c_str()));
        }
    }
}


// ============================================================================
// PRIVATE METHODS - Interval-Based Formula Evaluation
// ============================================================================

NumericRelaxedPlanningGraph::FormulaResult
NumericRelaxedPlanningGraph::evaluate_comparison_interval(
    ExprOperator op, const Interval& lhs, const Interval& rhs) const {

    switch (op) {
        case ExprOperator::LESS_EQUAL:
            if (lhs.upper <= rhs.lower) return FormulaResult::ALWAYS_TRUE;
            if (lhs.lower > rhs.upper)  return FormulaResult::ALWAYS_FALSE;
            return FormulaResult::UNKNOWN;

        case ExprOperator::LESS_THAN:
            if (lhs.upper < rhs.lower)  return FormulaResult::ALWAYS_TRUE;
            if (lhs.lower >= rhs.upper) return FormulaResult::ALWAYS_FALSE;
            return FormulaResult::UNKNOWN;

        case ExprOperator::GREATER_EQUAL:
            if (lhs.lower >= rhs.upper) return FormulaResult::ALWAYS_TRUE;
            if (lhs.upper < rhs.lower)  return FormulaResult::ALWAYS_FALSE;
            return FormulaResult::UNKNOWN;

        case ExprOperator::GREATER_THAN:
            if (lhs.lower > rhs.upper)  return FormulaResult::ALWAYS_TRUE;
            if (lhs.upper <= rhs.lower) return FormulaResult::ALWAYS_FALSE;
            return FormulaResult::UNKNOWN;

        case ExprOperator::EQUALS: {
            double eps = Config::instance().global.epsilon;
            // Both point intervals at same value
            if (std::abs(lhs.upper - lhs.lower) < eps &&
                std::abs(rhs.upper - rhs.lower) < eps &&
                std::abs(lhs.lower - rhs.lower) < eps)
                return FormulaResult::ALWAYS_TRUE;
            // Disjoint intervals
            if (lhs.upper < rhs.lower || rhs.upper < lhs.lower)
                return FormulaResult::ALWAYS_FALSE;
            return FormulaResult::UNKNOWN;
        }

        default:
            return FormulaResult::UNKNOWN;
    }
}

NumericRelaxedPlanningGraph::FormulaResult
NumericRelaxedPlanningGraph::evaluate_formula_interval(ExprID eid, int layer) const {
    if (!eid.valid()) return FormulaResult::UNKNOWN;

    const auto& pool = problem_.pool();

    // --- Boolean constant leaf ---
    if (pool.is_constant(eid) && pool.payload_is_bool(eid)) {
        return pool.payload_bool(eid) ? FormulaResult::ALWAYS_TRUE
                                      : FormulaResult::ALWAYS_FALSE;
    }

    // --- Boolean state variable (positive atom: "fluent is true") ---
    if (pool.is_state_variable(eid) && is_boolean_expression(eid)) {
        int fluent_id = find_grounded_fluent_id(eid);
        if (fluent_id < 0) return FormulaResult::UNKNOWN;
        auto it = layer_states_[layer].boolean_reachability.find(fluent_id);
        if (it == layer_states_[layer].boolean_reachability.end())
            return FormulaResult::ALWAYS_FALSE;  // not yet reachable
        switch (it->second) {
            case BooleanReachability::FALSE_ONLY: return FormulaResult::ALWAYS_FALSE;
            case BooleanReachability::TRUE_ONLY:  return FormulaResult::ALWAYS_TRUE;
            case BooleanReachability::BOTH:       return FormulaResult::UNKNOWN;
        }
        return FormulaResult::UNKNOWN;
    }

    // --- AND ---
    if (pool.is_and(eid)) {
        bool all_true = true;
        auto process = [&](ExprID child) -> bool {
            FormulaResult r = evaluate_formula_interval(child, layer);
            if (r == FormulaResult::ALWAYS_FALSE) return false;  // short-circuit
            if (r != FormulaResult::ALWAYS_TRUE) all_true = false;
            return true;
        };
        if (pool.has_head_and_arguments(eid)) {
            for (ExprID arg : pool.arguments(eid))
                if (!process(arg)) return FormulaResult::ALWAYS_FALSE;
        } else {
            for (ExprID child : pool.children(eid))
                if (!process(child)) return FormulaResult::ALWAYS_FALSE;
        }
        return all_true ? FormulaResult::ALWAYS_TRUE : FormulaResult::UNKNOWN;
    }

    // --- OR ---
    if (pool.is_or(eid)) {
        bool all_false = true;
        auto process = [&](ExprID child) -> bool {
            FormulaResult r = evaluate_formula_interval(child, layer);
            if (r == FormulaResult::ALWAYS_TRUE) return false;  // short-circuit
            if (r != FormulaResult::ALWAYS_FALSE) all_false = false;
            return true;
        };
        if (pool.has_head_and_arguments(eid)) {
            for (ExprID arg : pool.arguments(eid))
                if (!process(arg)) return FormulaResult::ALWAYS_TRUE;
        } else {
            for (ExprID child : pool.children(eid))
                if (!process(child)) return FormulaResult::ALWAYS_TRUE;
        }
        return all_false ? FormulaResult::ALWAYS_FALSE : FormulaResult::UNKNOWN;
    }

    // --- NOT ---
    if (pool.is_not(eid)) {
        ExprID child = pool.has_head_and_arguments(eid) ? pool.argument(eid, 0)
                                                         : pool.child(eid, 0);
        FormulaResult r = evaluate_formula_interval(child, layer);
        if (r == FormulaResult::ALWAYS_TRUE) return FormulaResult::ALWAYS_FALSE;
        if (r == FormulaResult::ALWAYS_FALSE) return FormulaResult::ALWAYS_TRUE;
        return FormulaResult::UNKNOWN;
    }

    // --- IMPLIES(A, B) = OR(NOT(A), B) ---
    if (pool.is_implies(eid)) {
        ExprID a = pool.has_head_and_arguments(eid) ? pool.argument(eid, 0) : pool.child(eid, 0);
        ExprID b = pool.has_head_and_arguments(eid) ? pool.argument(eid, 1) : pool.child(eid, 1);
        FormulaResult ra = evaluate_formula_interval(a, layer);
        FormulaResult rb = evaluate_formula_interval(b, layer);
        // NOT(A): flip ra
        FormulaResult not_a = (ra == FormulaResult::ALWAYS_TRUE) ? FormulaResult::ALWAYS_FALSE
                            : (ra == FormulaResult::ALWAYS_FALSE) ? FormulaResult::ALWAYS_TRUE
                            : FormulaResult::UNKNOWN;
        // OR(NOT(A), B)
        if (not_a == FormulaResult::ALWAYS_TRUE || rb == FormulaResult::ALWAYS_TRUE)
            return FormulaResult::ALWAYS_TRUE;
        if (not_a == FormulaResult::ALWAYS_FALSE && rb == FormulaResult::ALWAYS_FALSE)
            return FormulaResult::ALWAYS_FALSE;
        return FormulaResult::UNKNOWN;
    }

    // --- Function application: comparisons and IFF ---
    if (pool.is_function_application(eid)) {
        ExprOperator op = pool.op(eid);
        size_t nargs = pool.argument_count(eid);

        if (nargs == 2 && is_comparison_operator(op)) {
            ExprID lhs_eid = pool.argument(eid, 0);
            ExprID rhs_eid = pool.argument(eid, 1);

            // EQUALS between booleans: handle as IFF
            if (op == ExprOperator::EQUALS &&
                is_boolean_expression(lhs_eid) && is_boolean_expression(rhs_eid)) {
                FormulaResult rl = evaluate_formula_interval(lhs_eid, layer);
                FormulaResult rr = evaluate_formula_interval(rhs_eid, layer);
                // IFF: both same → true, both opposite → false
                if (rl == rr && rl != FormulaResult::UNKNOWN)
                    return FormulaResult::ALWAYS_TRUE;
                if ((rl == FormulaResult::ALWAYS_TRUE && rr == FormulaResult::ALWAYS_FALSE) ||
                    (rl == FormulaResult::ALWAYS_FALSE && rr == FormulaResult::ALWAYS_TRUE))
                    return FormulaResult::ALWAYS_FALSE;
                return FormulaResult::UNKNOWN;
            }

            // Numeric comparison
            Interval lhs = evaluate_interval(lhs_eid, layer);
            Interval rhs = evaluate_interval(rhs_eid, layer);
            return evaluate_comparison_interval(op, lhs, rhs);
        }

        // IFF (if represented as its own operator)
        if (nargs == 2 && op == ExprOperator::IFF) {
            ExprID a = pool.argument(eid, 0);
            ExprID b = pool.argument(eid, 1);
            FormulaResult ra = evaluate_formula_interval(a, layer);
            FormulaResult rb = evaluate_formula_interval(b, layer);
            if (ra == rb && ra != FormulaResult::UNKNOWN)
                return FormulaResult::ALWAYS_TRUE;
            if ((ra == FormulaResult::ALWAYS_TRUE && rb == FormulaResult::ALWAYS_FALSE) ||
                (ra == FormulaResult::ALWAYS_FALSE && rb == FormulaResult::ALWAYS_TRUE))
                return FormulaResult::ALWAYS_FALSE;
            return FormulaResult::UNKNOWN;
        }
    }

    // --- Fallback: assume possibly satisfiable ---
    return FormulaResult::UNKNOWN;
}

bool NumericRelaxedPlanningGraph::is_action_applicable_interval(
    const Action& action, int layer) const {
    total_interval_checks_++;
    if (!action.has_precondition()) return true;
    return evaluate_formula_interval(action.precondition_id(), layer)
        != FormulaResult::ALWAYS_FALSE;
}

bool NumericRelaxedPlanningGraph::are_goals_achievable_at_layer_interval(int layer) const {
    for (const Goal& goal : problem_.goals()) {
        if (evaluate_formula_interval(goal.goal_id(), layer) == FormulaResult::ALWAYS_FALSE)
            return false;
    }
    return true;
}

double NumericRelaxedPlanningGraph::extract_numeric_value(const z3::expr& z3_value) const {
    // Handle different Z3 value types
    if (z3_value.is_numeral()) {
        // Try to get as integer first
        int int_val;
        if (z3_value.is_int() && z3_value.is_numeral_i(int_val)) {
            return static_cast<double>(int_val);
        }

        // Try to get as rational
        if (z3_value.is_real()) {
            // Get numerator and denominator
            std::string val_str = z3_value.to_string();

            // Simple parsing for now - Z3 returns rationals as "num/den" or decimals
            size_t slash_pos = val_str.find('/');
            if (slash_pos != std::string::npos) {
                // Rational: parse num and den
                double num = std::stod(val_str.substr(0, slash_pos));
                double den = std::stod(val_str.substr(slash_pos + 1));
                return num / den;
            } else {
                // Decimal or integer
                return std::stod(val_str);
            }
        }
    }

    // Fallback: try to parse string representation
    std::string val_str = z3_value.to_string();
    try {
        return std::stod(val_str);
    } catch (...) {
        std::cerr << "Warning: Could not extract numeric value from Z3 expression: "
                  << val_str << std::endl;
        return 0.0;
    }
}

// ============================================================================
// PRIVATE METHODS - Helper Methods for Effects
// ============================================================================

std::vector<const EffectExpression*> NumericRelaxedPlanningGraph::get_effects_for_fluent(
    int fluent_id,
    const std::vector<const Action*>& actions) const {

    std::vector<const EffectExpression*> effects;

    // Get the fluent ExprID
    ExprID fluent_eid = problem_.grounded_fluent(fluent_id);

    // Look up in EPC index
    auto epc_it = epc_index_.find(fluent_eid);
    if (epc_it == epc_index_.end()) {
        return effects;  // No effects for this fluent
    }

    // Filter by applicable actions
    for (const auto& [action, effect_expr] : epc_it->second) {
        // Check if this action is in the applicable actions list
        for (const Action* app_action : actions) {
            if (app_action == action) {
                effects.push_back(effect_expr);
                break;
            }
        }
    }

    return effects;
}

// ============================================================================
// PRIVATE METHODS - Fixpoint Detection
// ============================================================================

bool NumericRelaxedPlanningGraph::is_fixpoint_reached() const {
    // Need at least 2 layers to compare
    if (layer_states_.size() < 2) {
        return false;
    }

    // Compare last two layers
    const auto& prev_layer = layer_states_[layer_states_.size() - 2];
    const auto& curr_layer = layer_states_[layer_states_.size() - 1];

    return prev_layer == curr_layer;
}

bool NumericRelaxedPlanningGraph::are_all_actions_reachable() const {
    // Due to monotonicity of relaxed planning graph, the last layer contains
    // all actions that will ever be reachable (once applicable, always applicable)
    if (action_layers_.empty()) {
        return false;
    }
    return action_layers_.back().size() >= problem_.actions().size();
}

// ============================================================================
// QUERY METHODS - Goals
// ============================================================================

bool NumericRelaxedPlanningGraph::are_goals_achievable() const {
    if (layer_states_.empty()) {
        return false;
    }

    int final_layer = static_cast<int>(layer_states_.size()) - 1;

    if (Config::instance().rpg.use_interval_checker) {
        return are_goals_achievable_at_layer_interval(final_layer);
    }

    // Create a fresh solver for goal checking
    z3::solver solver(ctx_);

    // Add final layer state constraints
    add_layer_constraints(solver, final_layer);

    // Add all goal expressions
    for (const Goal& goal : problem_.goals()) {
        z3::expr goal_expr = grounded_visitor_.convert_from_pool(goal.goal_id(), final_layer);
        solver.add(goal_expr);
    }

    // Check satisfiability
    total_smt_queries_++;
    z3::check_result result = solver.check();

    return result == z3::sat;
}

int NumericRelaxedPlanningGraph::get_minimum_steps_lower_bound() const {
    if (!are_goals_achievable()) {
        return -1;  // Goals not achievable
    }

    // Binary search for minimum layer where goals are achievable
    int left = 0;
    int right = static_cast<int>(layer_states_.size()) - 1;
    int min_layer = right;
    bool use_interval = Config::instance().rpg.use_interval_checker;

    while (left <= right) {
        int mid = (left + right) / 2;

        bool achievable;
        if (use_interval) {
            achievable = are_goals_achievable_at_layer_interval(mid);
        } else {
            z3::solver solver(ctx_);
            add_layer_constraints(solver, mid);

            for (const Goal& goal : problem_.goals()) {
                z3::expr goal_expr = grounded_visitor_.convert_from_pool(goal.goal_id(), mid);
                solver.add(goal_expr);
            }

            total_smt_queries_++;
            z3::check_result result = solver.check();
            achievable = (result == z3::sat);
        }

        if (achievable) {
            // Goals achievable at mid, try earlier
            min_layer = mid;
            right = mid - 1;
        } else {
            // Goals not achievable at mid, try later
            left = mid + 1;
        }
    }

    return min_layer;
}

// ============================================================================
// QUERY METHODS - Actions
// ============================================================================

const std::vector<const Action*>& NumericRelaxedPlanningGraph::get_actions_in_layer(int layer) const {
    if (layer >= 0 && layer < static_cast<int>(action_layers_.size())) {
        return action_layers_[layer];
    }
    return empty_action_vector_;
}

// ============================================================================
// ACHIEVER ANALYSIS SUPPORT
// ============================================================================

std::unordered_map<int, int> NumericRelaxedPlanningGraph::get_action_first_layers() const {
    std::unordered_map<int, int> first_layers;
    for (int layer = 0; layer < static_cast<int>(action_layers_.size()); ++layer) {
        for (const Action* action : action_layers_[layer]) {
            first_layers.try_emplace(action->id(), layer);
        }
    }
    return first_layers;
}

std::vector<const Action*> NumericRelaxedPlanningGraph::get_action_ordering() const {
    auto first_layers = get_action_first_layers();
    std::vector<const Action*> ordering;
    ordering.reserve(first_layers.size());
    for (const Action& a : problem_.actions()) {
        if (first_layers.count(a.id())) {
            ordering.push_back(&a);
        }
    }
    std::stable_sort(ordering.begin(), ordering.end(),
        [&](const Action* a, const Action* b) {
            return first_layers.at(a->id()) < first_layers.at(b->id());
        });
    return ordering;
}

std::unordered_map<ExprID, Interval> NumericRelaxedPlanningGraph::get_state_variable_bounds() const {
    std::unordered_map<ExprID, Interval> bounds;
    if (layer_states_.empty()) return bounds;

    int final_layer = static_cast<int>(layer_states_.size()) - 1;
    const auto& final_state = layer_states_[final_layer];

    // Numeric fluent bounds
    for (const auto& [fluent_id, nb] : final_state.numeric_bounds) {
        ExprID eid = problem_.grounded_fluent(fluent_id);
        bounds.emplace(eid, Interval(nb.lower, nb.upper));
    }

    return bounds;
}

// ============================================================================
// ACTION REMOVAL
// ============================================================================

std::vector<size_t> NumericRelaxedPlanningGraph::get_removable_action_indices() const {
    // Collect all actions that appear in any layer of the numeric RPG
    std::unordered_set<const Action*> reachable_actions;
    for (const auto& layer : action_layers_) {
        for (const Action* action : layer) {
            reachable_actions.insert(action);
        }
    }

    // Find actions that never appear in any layer
    std::vector<size_t> indices;
    for (size_t i = 0; i < problem_.action_count(); ++i) {
        if (!reachable_actions.count(&problem_.action(i))) {
            indices.push_back(i);
        }
    }

    return indices;
}

// ============================================================================
// DEBUG AND ANALYSIS
// ============================================================================

void NumericRelaxedPlanningGraph::print_statistics() const {
    std::cout << "\n=== Numeric Relaxed Planning Graph Statistics ===" << std::endl;
    std::cout << "Build time: " << build_time_ms_ << " ms" << std::endl;
    std::cout << "Total layers: " << layer_states_.size() << std::endl;
    std::cout << "Total SMT queries: " << total_smt_queries_ << std::endl;
    std::cout << "Total applicability checks: " << total_applicability_checks_ << std::endl;
    std::cout << "Total interval checks: " << total_interval_checks_ << std::endl;

    if (!layer_states_.empty()) {
        const auto& final_layer = layer_states_.back();
        std::cout << "Final layer Boolean fluents: " << final_layer.boolean_reachability.size() << std::endl;
        std::cout << "Final layer numeric fluents: " << final_layer.numeric_bounds.size() << std::endl;
    }

    // Compute reachable and unreachable actions
    std::unordered_set<const Action*> reachable_actions;
    for (const auto& layer_actions : action_layers_) {
        for (const Action* action : layer_actions) {
            reachable_actions.insert(action);
        }
    }

    int total_actions = problem_.actions().size();
    int reachable_count = reachable_actions.size();
    int unreachable_count = total_actions - reachable_count;

    std::cout << "Total actions: " << total_actions << std::endl;
    std::cout << "Reachable actions: " << reachable_count << std::endl;
    std::cout << "Unreachable actions: " << unreachable_count << std::endl;

    // List unreachable actions if any
    if (unreachable_count > 0) {
        std::cout << "\nUnreachable actions:" << std::endl;
        for (const Action& action : problem_.actions()) {
            if (reachable_actions.find(&action) == reachable_actions.end()) {
                std::cout << "  - " << action.name() << std::endl;
            }
        }
    }

    std::cout << "=================================================" << std::endl;
}

// ============================================================================
// DEBUG VISUALIZATION
// ============================================================================

void NumericRelaxedPlanningGraph::print_layer_summary(int layer) const {
    if (layer >= static_cast<int>(layer_states_.size())) {
        return;
    }

    const auto& layer_state = layer_states_[layer];
    const auto& applicable = (layer < static_cast<int>(action_layers_.size())) ?
        action_layers_[layer] : std::vector<const Action*>();

    // Count Boolean states
    int false_only = 0, true_only = 0, both = 0;
    for (const auto& [fluent_id, reach] : layer_state.boolean_reachability) {
        if (reach == BooleanReachability::FALSE_ONLY) false_only++;
        else if (reach == BooleanReachability::TRUE_ONLY) true_only++;
        else if (reach == BooleanReachability::BOTH) both++;
    }

    std::ostringstream component_name, bool_states;
    component_name << "RPG.Numeric L" << layer;
    bool_states << "F:" << false_only << "/T:" << true_only << "/B:" << both;

    Logger::instance().component(VerbosityLevel::VERBOSE, component_name.str(), {
        {"actions", std::to_string(applicable.size())},
        {"bool", std::to_string(layer_state.boolean_reachability.size()) + " (" + bool_states.str() + ")"},
        {"num", std::to_string(layer_state.numeric_bounds.size())}
    });
}

void NumericRelaxedPlanningGraph::print_action_applicability(
    int layer, const std::vector<const Action*>& applicable) const {

    std::cout << "\n[Layer " << layer << " - Action Applicability]" << std::endl;
    std::cout << "  Applicable actions (" << applicable.size() << "):" << std::endl;

    for (const Action* action : applicable) {
        std::cout << "    + " << action->name() << std::endl;
    }

    // Show blocked actions (all actions not in applicable list)
    std::unordered_set<const Action*> applicable_set(applicable.begin(), applicable.end());
    std::vector<const Action*> blocked;

    for (const Action& action : problem_.actions()) {
        if (applicable_set.find(&action) == applicable_set.end()) {
            blocked.push_back(&action);
        }
    }

    if (!blocked.empty()) {
        std::cout << "  Blocked actions (" << blocked.size() << "):" << std::endl;
        // Only show first few to avoid overwhelming output
        size_t show_count = std::min(blocked.size(), size_t(10));
        for (size_t i = 0; i < show_count; ++i) {
            std::cout << "    - " << blocked[i]->name() << std::endl;
        }
        if (blocked.size() > show_count) {
            std::cout << "    ... and " << (blocked.size() - show_count) << " more" << std::endl;
        }
    }

    // Show numeric fluent bounds at this layer
    if (layer < static_cast<int>(layer_states_.size())) {
        const auto& layer_state = layer_states_[layer];
        if (!layer_state.numeric_bounds.empty()) {
            std::cout << "  Numeric fluents (" << layer_state.numeric_bounds.size() << "):" << std::endl;
            for (const auto& [fluent_id, bounds] : layer_state.numeric_bounds) {
                std::cout << "    " << problem_.pool().to_string(problem_.grounded_fluent(fluent_id)) << ": "
                          << "[" << bounds.lower << ", " << bounds.upper << "]" << std::endl;
            }
        }
    }
}

void NumericRelaxedPlanningGraph::print_layer_delta(int prev_layer, int curr_layer) const {
    if (prev_layer >= static_cast<int>(layer_states_.size()) ||
        curr_layer >= static_cast<int>(layer_states_.size())) {
        return;
    }

    const auto& prev_state = layer_states_[prev_layer];
    const auto& curr_state = layer_states_[curr_layer];

    std::cout << "\n[Layer " << prev_layer << " → " << curr_layer << " Delta]" << std::endl;

    // Newly enabled actions
    std::vector<const Action*> new_actions;
    if (prev_layer < static_cast<int>(action_layers_.size()) &&
        curr_layer < static_cast<int>(action_layers_.size())) {

        const auto& prev_actions = action_layers_[prev_layer];
        const auto& curr_actions = action_layers_[curr_layer];

        // Create set of previous layer actions for fast lookup
        std::unordered_set<const Action*> prev_action_set(prev_actions.begin(), prev_actions.end());

        // Find actions in current layer but not in previous layer
        for (const Action* action : curr_actions) {
            if (prev_action_set.find(action) == prev_action_set.end()) {
                new_actions.push_back(action);
            }
        }
    }

    if (!new_actions.empty()) {
        std::cout << "  Newly enabled actions (" << new_actions.size() << "):" << std::endl;
        for (const Action* action : new_actions) {
            std::cout << "    + " << action->name() << std::endl;
        }
    }

    // Boolean transitions
    std::vector<std::pair<int, std::pair<BooleanReachability, BooleanReachability>>> bool_changes;
    for (const auto& [fluent_id, curr_reach] : curr_state.boolean_reachability) {
        auto prev_it = prev_state.boolean_reachability.find(fluent_id);
        if (prev_it != prev_state.boolean_reachability.end() && prev_it->second != curr_reach) {
            bool_changes.push_back({fluent_id, {prev_it->second, curr_reach}});
        }
    }

    if (!bool_changes.empty()) {
        std::cout << "  Boolean transitions (" << bool_changes.size() << "):" << std::endl;
        for (const auto& [fluent_id, transition] : bool_changes) {
            // Get fluent expression by index
            std::cout << "    " << problem_.pool().to_string(problem_.grounded_fluent(fluent_id)) << ": "
                      << reachability_to_string(transition.first) << " → "
                      << reachability_to_string(transition.second) << std::endl;
        }
    }

    // Numeric bound changes
    auto epsilon = Config::instance().global.epsilon;
    std::vector<std::tuple<int, NumericBounds, NumericBounds>> num_changes;
    for (const auto& [fluent_id, curr_bounds] : curr_state.numeric_bounds) {
        auto prev_it = prev_state.numeric_bounds.find(fluent_id);
        if (prev_it != prev_state.numeric_bounds.end() && prev_it->second != curr_bounds) {
            num_changes.push_back({fluent_id, prev_it->second, curr_bounds});
        }
    }

    if (!num_changes.empty()) {
        std::cout << "  Numeric bound changes (" << num_changes.size() << "):" << std::endl;
        for (const auto& [fluent_id, prev_bounds, curr_bounds] : num_changes) {
            // Get fluent expression by index
            std::cout << "    " << problem_.pool().to_string(problem_.grounded_fluent(fluent_id)) << ": "
                      << "[" << prev_bounds.lower << ", " << prev_bounds.upper << "] → "
                      << "[" << curr_bounds.lower << ", " << curr_bounds.upper << "]" << std::endl;
        }
    }

    if (bool_changes.empty() && num_changes.empty()) {
        std::cout << "  No changes (fixpoint)" << std::endl;
    }
}

} // namespace rantanplan
