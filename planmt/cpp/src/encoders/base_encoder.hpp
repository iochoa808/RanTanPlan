#pragma once

#include "../problem/problem.hpp"
#include "../problem/plan.hpp"
#include "../problem/expression.hpp"
#include "../problem/effect_expression.hpp"
#include "z3_variable_factory.hpp"
#include "parallelism/parallelism_strategy.hpp"
#include <z3++.h>

#include <memory>
#include <string>

namespace planmt {

// Base class for state-space encodings to provide a common interface
class BaseEncoder {
public:
    virtual ~BaseEncoder() = default;

    // Encoding steps
    virtual std::shared_ptr<z3::expr> encode_initial_state() = 0;
    virtual std::shared_ptr<z3::expr> encode_actions(int t) = 0;
    virtual std::shared_ptr<z3::expr> encode_frames(int t) = 0;
    virtual std::shared_ptr<z3::expr> encode_goal(int t) = 0;
    virtual std::shared_ptr<z3::expr> encode_parallelism(int t) = 0;
    virtual std::shared_ptr<z3::expr> encode_symmetries(int t) = 0;
    
    // Parallelism management
    virtual void set_parallelism_strategy(std::unique_ptr<ParallelismStrategy> strategy) = 0;
    virtual std::string get_parallelism_strategy_name() const = 0;
    virtual const ParallelismStrategy* get_parallelism_strategy() const = 0; // for plan extraction
    
    // Access to variable factory for plan extraction
    virtual Z3VariableFactory& get_variable_factory() = 0;
    virtual const Z3VariableFactory& get_variable_factory() const = 0;
    
    // Helper functions to convert expressions/effects to Z3 using visitor (for propagators)
    virtual std::optional<z3::expr> convert_expression_to_z3(const Expression& expr, int timestep = -1) = 0;
    virtual std::optional<z3::expr> convert_effect_to_z3(const EffectExpression& effect, int timestep) = 0;
    
    // Plan extraction from Z3 model
    virtual Plan extract_plan(const z3::model& model, int max_timestep) const = 0;
};

} // namespace planmt
