#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "../problem/effect_expression.hpp"
#include "../symmetries/smt_symmetry_checker.hpp"
#include "z3_variable_factory.hpp"
#include "parallelism/parallelism_strategy.hpp"
#include <z3++.h>

#include <memory>
#include <string>
#include <vector>

namespace rantanplan {

// Base class for state-space encodings to provide a common interface
class BaseEncoder {
public:
    virtual ~BaseEncoder() = default;

    // Pipeline data (set once before search, used by encode_symmetries)
    virtual void set_symmetry_data(std::vector<SymmetryInfo> data) { symmetry_data_ = std::move(data); }

    // Encoding steps
    virtual std::shared_ptr<z3::expr> encode_initial_state() = 0;
    virtual std::shared_ptr<z3::expr> encode_actions(int t) = 0;
    virtual std::shared_ptr<z3::expr> encode_frames(int t) = 0;
    virtual std::shared_ptr<z3::expr> encode_goal(int t) = 0;
    virtual std::shared_ptr<z3::expr> encode_parallelism(int t) = 0;
    virtual std::shared_ptr<z3::expr> encode_symmetries(int t) = 0;

    /**
     * @brief Encode the prefix-monotone (front-loading) symmetry-breaking constraint.
     *
     * Adds:  (∀a. ¬a@t)  →  (∀a. ¬a@(t+1))
     *
     * This chains across all t during search, forcing all active action steps to
     * be contiguous at the front of the plan. Combined with at-most-one (pble)
     * parallelism semantics, this guarantees that if a valid plan of length k < h
     * exists, the solver can represent it with the actions at 0..k-1 and empty steps
     * at k..h-1. Frame axioms then propagate the goal state to timestep h, so a
     * single goal literal at h witnesses any plan length in the batch [prev_h+1..h].
     *
     * Only called by SequentialPlanner; DoubleTailPlanner has structurally disjoint
     * forward/backward stacks that cannot use a single uniform front-loading chain.
     */
    virtual std::shared_ptr<z3::expr> encode_prefix_monotone(int t) = 0;
    
    // Parallelism management
    virtual void set_parallelism_strategy(std::unique_ptr<ParallelismStrategy> strategy) = 0;
    virtual std::string get_parallelism_strategy_name() const = 0;
    virtual const ParallelismStrategy* get_parallelism_strategy() const = 0; // for plan extraction
    
    // Access to variable factory for plan extraction
    virtual Z3VariableFactory& get_variable_factory() = 0;
    virtual const Z3VariableFactory& get_variable_factory() const = 0;
    
    // Helper functions to convert expressions/effects to Z3 using visitor (for propagators)
    virtual z3::expr convert_expr_id_to_z3(ExprID id, int timestep = -1) = 0;
    virtual z3::expr convert_effect_to_z3(const EffectExpression& effect, int timestep) = 0;
    
    // Plan extraction from Z3 model
    virtual Plan extract_plan(const z3::model& model, int max_timestep) const = 0;

protected:
    std::vector<SymmetryInfo> symmetry_data_;
};

} // namespace rantanplan
