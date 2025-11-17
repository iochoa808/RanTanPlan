#include "numeric_relaxed_planning_graph.hpp"
#include "../config/config.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>

namespace planmt {

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

NumericRelaxedPlanningGraph::NumericRelaxedPlanningGraph(Problem& problem, z3::context& ctx)
    : problem_(problem),
      ctx_(ctx),
      z3_optimizer_(std::make_unique<z3::optimize>(ctx)),
      variable_factory_(ctx),
      grounded_visitor_(ctx, &problem, &variable_factory_),
      max_layers_(Config::instance().planner.max_steps),  // Read from config
      batch_action_applicability_(false),  // Default: individual queries per action
      build_time_ms_(0.0),
      total_smt_queries_(0),
      total_optimization_queries_(0),
      total_applicability_checks_(0) {

    // Build EPC index for efficient effect lookup
    build_epc_index();

    // Classify fluents into Boolean vs numeric
    classify_fluents();

    auto& config = Config::instance();
    if (config.is_debug()) {
        std::cout << "NumericRelaxedPlanningGraph initialized:" << std::endl;
        std::cout << "  - Total fluents: " << problem_.grounded_fluents().size() << std::endl;
        std::cout << "  - Boolean fluents: " << boolean_fluent_ids_.size() << std::endl;
        std::cout << "  - Numeric fluents: " << numeric_fluent_ids_.size() << std::endl;
        std::cout << "  - Total actions: " << problem_.actions().size() << std::endl;
        std::cout << "  - Max layers: " << max_layers_ << std::endl;
    }
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

bool NumericRelaxedPlanningGraph::is_boolean_expression(const Expression& expr) const {
    // Check if expression type is Boolean
    return expr.type() != nullptr && expr.type()->is_bool();
}

bool NumericRelaxedPlanningGraph::is_numeric_expression(const Expression& expr) const {
    // Check if expression type is numeric (int or real)
    return expr.type() != nullptr &&
           (expr.type()->is_int() || expr.type()->is_real());
}

int NumericRelaxedPlanningGraph::find_grounded_fluent_id(const Expression& fluent) const {
    // Use Problem's find_grounded_fluent_index for O(1) lookup
    int index = problem_.find_grounded_fluent_index(fluent);
    if (index < 0) {
        std::cerr << "Error: Fluent not found in grounded fluents: "
                  << fluent.to_string() << std::endl;
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
    for (const Expression& fluent : problem_.grounded_fluents()) {
        epc_index_[fluent] = std::vector<std::pair<const Action*, const EffectExpression*>>();
    }

    // Add action effects to the fluents that can be modified
    for (const Action& action : problem_.actions()) {
        for (const Effect& effect : action.effects()) {
            const EffectExpression& eff_expr = effect.effect_expression();
            const Expression& fluent = eff_expr.fluent();
            epc_index_[fluent].emplace_back(&action, &eff_expr);
        }
    }

    auto& config = Config::instance();
    if (config.is_debug()) {
        std::cout << "EPC index built: " << epc_index_.size() << " fluents indexed" << std::endl;
        size_t total_effects = 0;
        for (const auto& [fluent, effects] : epc_index_) {
            total_effects += effects.size();
        }
        std::cout << "  Total effect entries: " << total_effects << std::endl;
    }
}

void NumericRelaxedPlanningGraph::classify_fluents() {
    // Classify each grounded fluent as Boolean or numeric based on type
    boolean_fluent_ids_.clear();
    numeric_fluent_ids_.clear();

    int fluent_id = 0;
    for (const Expression& fluent : problem_.grounded_fluents()) {
        if (is_boolean_expression(fluent)) {
            boolean_fluent_ids_.insert(fluent_id);
        } else if (is_numeric_expression(fluent)) {
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

    // Process each assignment in the initial state
    for (const auto& assignment : problem_.initial_state()) {
        const Expression& fluent = assignment.fluent();
        int fluent_id = find_grounded_fluent_id(fluent);
        if (fluent_id < 0) {
            continue;  // Skip if not found
        }

        const Expression& value = assignment.value();

        if (boolean_fluent_ids_.contains(fluent_id)) {
            // Boolean fluent: check if value is true or false
            if (value.is_constant() && value.is_atom() && value.value().is_boolean()) {
                bool is_true = value.value().boolean();
                initial_layer.boolean_reachability[fluent_id] =
                    is_true ? BooleanReachability::TRUE_ONLY
                            : BooleanReachability::FALSE_ONLY;
            } else {
                std::cerr << "Warning: Boolean fluent has non-Boolean value in initial state: "
                          << fluent.to_string() << std::endl;
                initial_layer.boolean_reachability[fluent_id] = BooleanReachability::FALSE_ONLY;
            }
        } else if (numeric_fluent_ids_.contains(fluent_id)) {
            // Numeric fluent: extract value and create point bounds
            if (value.is_constant() && value.is_atom()) {
                double numeric_value = 0.0;
                if (value.value().is_integer()) {
                    numeric_value = static_cast<double>(value.value().integer());
                } else if (value.value().is_real()) {
                    numeric_value = value.value().real().to_double();
                } else {
                    std::cerr << "Warning: Numeric fluent has non-numeric value in initial state: "
                              << fluent.to_string() << std::endl;
                }
                initial_layer.numeric_bounds[fluent_id] = NumericBounds(numeric_value);
            } else {
                std::cerr << "Warning: Numeric fluent has non-constant value in initial state: "
                          << fluent.to_string() << std::endl;
                initial_layer.numeric_bounds[fluent_id] = NumericBounds(0.0);
            }
        }
    }

    // Store layer 0
    layer_states_.push_back(std::move(initial_layer));
    action_layers_.push_back(std::vector<const Action*>());  // No actions at layer 0

    auto& config = Config::instance();
    if (config.is_debug()) {
        std::cout << "Layer 0 initialized:" << std::endl;
        std::cout << "  - Boolean fluents: " << layer_states_[0].boolean_reachability.size() << std::endl;
        std::cout << "  - Numeric fluents: " << layer_states_[0].numeric_bounds.size() << std::endl;
    }
}

// ============================================================================
// MAIN INTERFACE
// ============================================================================

bool NumericRelaxedPlanningGraph::build() {
    auto& config = Config::instance();
    auto start_time = std::chrono::high_resolution_clock::now();

    // Initialize layer 0 from initial state
    initialize_layer_0();

    if (config.is_verbose()) {
        std::cout << "NumericRelaxedPlanningGraph::build() - Starting layer expansion" << std::endl;
    }

    // Debug: Print initial layer (layer 0)
    if (config.is_debug()) {
        print_layer_summary(0);
    }

    // Layer-by-layer construction until fixpoint or max_layers
    for (int layer = 0; layer < max_layers_; ++layer) {
        if (config.is_verbose()) {
            std::cout << "\n--- Building layer " << (layer + 1) << " ---" << std::endl;
        }

        // Compute applicable actions using SMT-based checking
        std::vector<const Action*> applicable_actions = compute_applicable_actions(layer);

        // Debug: Print action applicability
        //if (config.is_debug()) {
        //    print_action_applicability(layer, applicable_actions);
        //}

        if (config.is_verbose()) {
            std::cout << "  Found " << applicable_actions.size() << " applicable actions" << std::endl;
        }

        // If no actions are applicable and we've already built at least one layer, we're done
        if (applicable_actions.empty() && layer > 0) {
            if (config.is_verbose()) {
                std::cout << "  No applicable actions - fixpoint reached" << std::endl;
            }
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

        // Debug: Print layer summary and delta
        if (config.is_debug()) {
            //print_layer_summary(layer + 1);
            print_layer_delta(layer, layer + 1);
        }

        if (config.is_verbose()) {
            const auto& new_layer = layer_states_[layer + 1];
            std::cout << "  Layer " << (layer + 1) << " created with:" << std::endl;
            std::cout << "    Boolean fluents: " << new_layer.boolean_reachability.size() << std::endl;
            std::cout << "    Numeric fluents: " << new_layer.numeric_bounds.size() << std::endl;

            // Count how many Boolean fluents are in each state
            int false_only_count = 0;
            int true_only_count = 0;
            int both_count = 0;
            for (const auto& [fluent_id, reach] : new_layer.boolean_reachability) {
                if (reach == BooleanReachability::FALSE_ONLY) false_only_count++;
                else if (reach == BooleanReachability::TRUE_ONLY) true_only_count++;
                else if (reach == BooleanReachability::BOTH) both_count++;
            }
            std::cout << "    Boolean states: FALSE_ONLY=" << false_only_count
                      << ", TRUE_ONLY=" << true_only_count
                      << ", BOTH=" << both_count << std::endl;
        }

        // Check for fixpoint
        if (is_fixpoint_reached()) {
            if (config.is_verbose()) {
                std::cout << "  Fixpoint reached - terminating layer expansion" << std::endl;
            }
            break;
        }

        // Early termination if goals are achievable (if enabled)
        // Note: This is sound for satisficing planning but may be unsound for optimal planning
        if (config.global.rpg_early_termination && are_goals_achievable()) {
            if (config.is_verbose()) {
                std::cout << "  Goals achievable - early termination" << std::endl;
            }
            break;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    build_time_ms_ = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (config.is_verbose()) {
        std::cout << "\nNumericRelaxedPlanningGraph::build() - Completed" << std::endl;
        std::cout << "  Total layers: " << layer_states_.size() << std::endl;
        std::cout << "  Build time: " << build_time_ms_ << " ms" << std::endl;
    }

    // Check if goals are achievable in the final layer
    return are_goals_achievable();
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

        // Get fluent expression for EPC lookup
        const Expression& fluent = problem_.grounded_fluents()[fluent_id];

        // Find all effects from applicable actions that modify this fluent
        auto epc_it = epc_index_.find(fluent);
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

    if (config.is_debug()) {
        std::cout << "  Boolean effects propagated for " << boolean_fluent_ids_.size()
                  << " fluents" << std::endl;
    }
}

void NumericRelaxedPlanningGraph::apply_boolean_effect(
    const EffectExpression& effect,
    BooleanReachability current_state,
    BooleanReachability& next_state,
    int layer) const {

    // Determine if this is a positive effect (sets to true) or negative (sets to false)
    // We need to check the effect value expression
    const Expression& value_expr = effect.value();

    bool is_positive_effect = false;
    bool is_negative_effect = false;

    // Check if the effect value is a Boolean constant
    if (value_expr.is_constant() && value_expr.is_atom() && value_expr.value().is_boolean()) {
        bool effect_value = value_expr.value().boolean();
        is_positive_effect = effect_value;   // Sets to true
        is_negative_effect = !effect_value;  // Sets to false
    } else {
        // Non-constant Boolean expression - conservatively assume it could be either
        // This is a simplification; proper handling would require SMT analysis
        is_positive_effect = true;
        is_negative_effect = true;
    }

    // Apply delete-relaxation semantics based on current state and effect type
    // State transitions (from header file):
    // - FALSE_ONLY + positive effect → BOTH
    // - FALSE_ONLY + negative effect → FALSE_ONLY (no change)
    // - TRUE_ONLY + positive effect → TRUE_ONLY (no change)
    // - TRUE_ONLY + negative effect → BOTH
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

    auto& config = Config::instance();
    const auto& prev_state = layer_states_[prev_layer];
    auto& next_state = layer_states_[next_layer];

    // Start by copying previous layer's numeric bounds (frame axiom - persistence)
    next_state.numeric_bounds = prev_state.numeric_bounds;

    // For each numeric fluent, compute new bounds considering all effects
    for (int fluent_id : numeric_fluent_ids_) {
        NumericBounds new_bounds = compute_single_variable_bounds(
            fluent_id, applicable_actions, prev_layer, next_layer);

        next_state.numeric_bounds[fluent_id] = new_bounds;
    }

    if (config.is_debug()) {
        std::cout << "  Numeric bounds computed for " << numeric_fluent_ids_.size()
                  << " fluents" << std::endl;
    }
}

NumericRelaxedPlanningGraph::NumericBounds NumericRelaxedPlanningGraph::compute_single_variable_bounds(
    int fluent_id,
    const std::vector<const Action*>& applicable_actions,
    int prev_layer,
    int next_layer) const {

    // Get the fluent expression
    const Expression& fluent = problem_.grounded_fluents()[fluent_id];

    // Get current bounds (for persistence option)
    NumericBounds current_bounds = NumericBounds(0.0);
    auto it = layer_states_[prev_layer].numeric_bounds.find(fluent_id);
    if (it != layer_states_[prev_layer].numeric_bounds.end()) {
        current_bounds = it->second;
    }

    // Get all effects for this fluent
    std::vector<const EffectExpression*> effects = get_effects_for_fluent(fluent_id, applicable_actions);

    // If no effects, bounds persist (frame axiom)
    if (effects.empty()) {
        return current_bounds;
    }

    // Query 1: Minimize fluent value
    double lower_bound = compute_bound_optimization(fluent_id, effects, prev_layer, next_layer, true);

    // Query 2: Maximize fluent value
    double upper_bound = compute_bound_optimization(fluent_id, effects, prev_layer, next_layer, false);

    return NumericBounds(lower_bound, upper_bound);
}

double NumericRelaxedPlanningGraph::compute_bound_optimization(
    int fluent_id,
    const std::vector<const EffectExpression*>& effects,
    int prev_layer,
    int next_layer,
    bool minimize) const {

    // Create a fresh optimizer for this query
    z3::optimize optimizer(ctx_);

    // Add previous layer state constraints
    z3::solver dummy_solver(ctx_); // We need a solver for add_layer_constraints
    add_layer_constraints(dummy_solver, prev_layer);

    // Copy constraints from dummy solver to optimizer
    z3::expr_vector assumptions = dummy_solver.assertions();
    for (unsigned i = 0; i < assumptions.size(); ++i) {
        optimizer.add(assumptions[i]);
    }

    // Get the fluent at the next layer
    const Expression& fluent = problem_.grounded_fluents()[fluent_id];
    z3::expr fluent_next = convert_expression_to_z3(fluent, next_layer);

    // Get current bounds for persistence
    NumericBounds current_bounds = NumericBounds(0.0);
    auto it = layer_states_[prev_layer].numeric_bounds.find(fluent_id);
    if (it != layer_states_[prev_layer].numeric_bounds.end()) {
        current_bounds = it->second;
    }

    // Build disjunction: fluent' = persistence OR effect1 OR effect2 OR ...
    z3::expr_vector effect_options(ctx_);

    // Option 1: Persistence (fluent' = fluent_current_value)
    // Since we have bounds, we allow any value in the current bounds
    z3::expr persist_lower = (fluent_next >= ctx_.real_val(std::to_string(current_bounds.lower).c_str()));
    z3::expr persist_upper = (fluent_next <= ctx_.real_val(std::to_string(current_bounds.upper).c_str()));
    effect_options.push_back(persist_lower && persist_upper);

    // Option 2+: Each effect from applicable actions
    for (const EffectExpression* effect_expr : effects) {
        z3::expr effect_value = convert_effect_value_to_z3(*effect_expr, prev_layer);
        
        // Build the constraint based on effect kind
        z3::expr effect_constraint = ctx_.bool_val(false);  // default: impossible
        
        switch (effect_expr->kind()) {
            case EffectExpression::Kind::ASSIGN:
                // ASSIGN: fluent' = value
                effect_constraint = (fluent_next == effect_value);
                break;
                
            case EffectExpression::Kind::INCREASE: {
                // INCREASE: fluent' = fluent + value
                z3::expr fluent_prev = convert_expression_to_z3(fluent, prev_layer);
                effect_constraint = (fluent_next == fluent_prev + effect_value);
                break;
            }
                
            case EffectExpression::Kind::DECREASE: {
                // DECREASE: fluent' = fluent - value
                z3::expr fluent_prev = convert_expression_to_z3(fluent, prev_layer);
                effect_constraint = (fluent_next == fluent_prev - effect_value);
                break;
            }
        }
        
        effect_options.push_back(effect_constraint);
    }

    // Add the disjunction constraint
    optimizer.add(z3::mk_or(effect_options));

    // Set objective: minimize or maximize
    z3::optimize::handle objective_handle = minimize ?
        optimizer.minimize(fluent_next) :
        optimizer.maximize(fluent_next);

    // Solve
    total_optimization_queries_++;
    z3::check_result result = optimizer.check();

    if (result == z3::sat) {
        z3::model model = optimizer.get_model();
        // For minimize: lower() gives minimum, for maximize: upper() gives maximum
        z3::expr optimal_value = minimize ? optimizer.lower(objective_handle) : optimizer.upper(objective_handle);

        return extract_numeric_value(optimal_value);
    } else {
        // If UNSAT, return current bound (shouldn't happen in relaxed graph)
        std::cerr << "Warning: UNSAT when computing numeric bounds for fluent " << fluent_id << std::endl;
        return minimize ? current_bounds.lower : current_bounds.upper;
    }
}

// ============================================================================
// PRIVATE METHODS - Action Applicability
// ============================================================================

std::vector<const Action*> NumericRelaxedPlanningGraph::compute_applicable_actions(int layer) const {
    // Choose between batch or individual based on configuration
    if (batch_action_applicability_) {
        return compute_applicable_actions_batch(layer);
    } else {
        return compute_applicable_actions_individual(layer);
    }
}

std::vector<const Action*> NumericRelaxedPlanningGraph::compute_applicable_actions_individual(int layer) const {
    auto& config = Config::instance();
    std::vector<const Action*> applicable_actions;

    // For each action, check if its precondition is satisfiable given the layer state
    for (const Action& action : problem_.actions()) {
        if (is_action_applicable_smt(action, layer)) {
            applicable_actions.push_back(&action);
        }
    }

    if (config.is_debug()) {
        std::cout << "  Found " << applicable_actions.size() << " applicable actions at layer "
                  << layer << " (individual queries)" << std::endl;
    }

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

        // action_var → precondition
        z3::expr precondition = convert_expression_to_z3(action.precondition(), layer);
        solver.add(z3::implies(action_var, precondition));
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

    if (config.is_debug()) {
        std::cout << "  Found " << applicable_actions.size() << " applicable actions at layer "
                  << layer << " (batch query)" << std::endl;
    }

    return applicable_actions;
}

bool NumericRelaxedPlanningGraph::is_action_applicable_smt(const Action& action, int layer) const {
    // Create a fresh solver for this query
    z3::solver solver(ctx_);

    // Add layer state constraints
    add_layer_constraints(solver, layer);

    // Add action precondition
    z3::expr precondition = convert_expression_to_z3(action.precondition(), layer);
    solver.add(precondition);

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
        const Expression& fluent = problem_.grounded_fluents()[fluent_id];
        z3::expr fluent_z3 = convert_expression_to_z3(fluent, layer);

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

    // For each numeric fluent, add bound constraints
    for (const auto& [fluent_id, bounds] : layer_state.numeric_bounds) {
        const Expression& fluent = problem_.grounded_fluents()[fluent_id];
        z3::expr fluent_z3 = convert_expression_to_z3(fluent, layer);

        // Add lower and upper bound constraints
        solver.add(fluent_z3 >= ctx_.real_val(std::to_string(bounds.lower).c_str()));
        solver.add(fluent_z3 <= ctx_.real_val(std::to_string(bounds.upper).c_str()));
    }
}

z3::expr NumericRelaxedPlanningGraph::convert_expression_to_z3(const Expression& expr, int layer) const {
    // Use the grounded visitor to convert the expression
    grounded_visitor_.clear();
    grounded_visitor_.set_timestep(layer);

    // Visit the expression using the visitor pattern
    accept_visitor(expr, grounded_visitor_);

    grounded_visitor_.clear_timestep();

    // Get the result
    if (grounded_visitor_.has_result()) {
        return grounded_visitor_.get_expression();
    } else {
        // Fallback: return a fresh Boolean constant if no result
        std::cerr << "Warning: No result from visitor for expression: " << expr.to_string() << std::endl;
        return ctx_.bool_const("error");
    }
}

z3::expr NumericRelaxedPlanningGraph::convert_effect_value_to_z3(const EffectExpression& effect_expr, int layer) const {
    // Get the value expression from the effect
    const Expression& value = effect_expr.value();

    // Convert to Z3
    return convert_expression_to_z3(value, layer);
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

    // Get the fluent expression
    const Expression& fluent = problem_.grounded_fluents()[fluent_id];

    // Look up in EPC index
    auto epc_it = epc_index_.find(fluent);
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

// ============================================================================
// QUERY METHODS - Goals
// ============================================================================

bool NumericRelaxedPlanningGraph::are_goals_achievable() const {
    if (layer_states_.empty()) {
        return false;
    }

    // Use the last layer for goal checking
    int final_layer = static_cast<int>(layer_states_.size()) - 1;

    // Create a fresh solver for goal checking
    z3::solver solver(ctx_);

    // Add final layer state constraints
    add_layer_constraints(solver, final_layer);

    // Add all goal expressions
    for (const Goal& goal : problem_.goals()) {
        z3::expr goal_expr = convert_expression_to_z3(goal.goal_expression(), final_layer);
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

    while (left <= right) {
        int mid = (left + right) / 2;

        // Check if goals are achievable at layer mid
        z3::solver solver(ctx_);
        add_layer_constraints(solver, mid);

        for (const Goal& goal : problem_.goals()) {
            z3::expr goal_expr = convert_expression_to_z3(goal.goal_expression(), mid);
            solver.add(goal_expr);
        }

        total_smt_queries_++;
        z3::check_result result = solver.check();

        if (result == z3::sat) {
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
// ACTION REMOVAL
// ============================================================================

std::vector<const Action*> NumericRelaxedPlanningGraph::get_removable_actions() const {
    // Collect all actions that appear in any layer of the numeric RPG
    std::unordered_set<const Action*> reachable_actions;
    for (const auto& layer : action_layers_) {
        for (const Action* action : layer) {
            reachable_actions.insert(action);
        }
    }

    // Find actions that never appear in any layer
    // These are unreachable in the numeric relaxed planning graph, meaning:
    // 1. Their preconditions (Boolean + numeric) are never satisfied
    // 2. They can be safely removed from the problem
    std::vector<const Action*> removable_actions;
    for (const Action& action : problem_.actions()) {
        if (reachable_actions.find(&action) == reachable_actions.end()) {
            removable_actions.push_back(&action);
        }
    }

    return removable_actions;
}

size_t NumericRelaxedPlanningGraph::remove_unreachable_actions() {
    // Get actions that can be safely removed
    auto removable_actions = get_removable_actions();

    if (removable_actions.empty()) {
        return 0;
    }

    // Sort by index in descending order to maintain index validity during removal
    std::vector<size_t> indices_to_remove;
    for (const Action* action : removable_actions) {
        // Find the index of this action
        for (size_t i = 0; i < problem_.action_count(); ++i) {
            if (&problem_.action(i) == action) {
                indices_to_remove.push_back(i);
                break;
            }
        }
    }

    // Sort indices in descending order
    std::sort(indices_to_remove.begin(), indices_to_remove.end(), std::greater<size_t>());

    // Remove actions from highest index to lowest
    size_t removed_count = 0;
    for (size_t index : indices_to_remove) {
        if (problem_.remove_action(index)) {
            removed_count++;
        }
    }

    return removed_count;
}

// ============================================================================
// DEBUG AND ANALYSIS
// ============================================================================

void NumericRelaxedPlanningGraph::print_statistics() const {
    std::cout << "\n=== Numeric Relaxed Planning Graph Statistics ===" << std::endl;
    std::cout << "Build time: " << build_time_ms_ << " ms" << std::endl;
    std::cout << "Total layers: " << layer_states_.size() << std::endl;
    std::cout << "Total SMT queries: " << total_smt_queries_ << std::endl;
    std::cout << "Total optimization queries: " << total_optimization_queries_ << std::endl;
    std::cout << "Total applicability checks: " << total_applicability_checks_ << std::endl;

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

    std::cout << "[Layer " << layer << "] "
              << "Actions: " << applicable.size() << " | "
              << "Bool: F=" << false_only << " T=" << true_only << " B=" << both << " | "
              << "Num: " << layer_state.numeric_bounds.size()
              << std::endl;
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
                const Expression& fluent = problem_.grounded_fluents()[fluent_id];
                std::cout << "    " << fluent.to_string() << ": "
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
            const Expression& fluent = problem_.grounded_fluents()[fluent_id];
            std::cout << "    " << fluent.to_string() << ": "
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
            const Expression& fluent = problem_.grounded_fluents()[fluent_id];
            std::cout << "    " << fluent.to_string() << ": "
                      << "[" << prev_bounds.lower << ", " << prev_bounds.upper << "] → "
                      << "[" << curr_bounds.lower << ", " << curr_bounds.upper << "]" << std::endl;
        }
    }

    if (bool_changes.empty() && num_changes.empty()) {
        std::cout << "  No changes (fixpoint)" << std::endl;
    }
}

} // namespace planmt
