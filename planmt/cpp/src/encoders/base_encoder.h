#pragma once

#include "../problem/problem.h"
#include "z3_variable_factory.h"
#include "parallelism/parallelism_strategy.h"
#include "parallelism/parallelism_factory.h"
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
    
    // Parallelism management
    virtual void set_parallelism_strategy(ParallelismFactory::ParallelismType type) = 0;
    virtual void set_parallelism_strategy(const std::string& strategy_name) = 0;
    virtual std::string get_parallelism_strategy_name() const = 0;
    virtual const ParallelismStrategy* get_parallelism_strategy() const = 0; // for plan extraction
    
    // Access to variable factory for plan extraction
    virtual Z3VariableFactory& get_variable_factory() = 0;
    virtual const Z3VariableFactory& get_variable_factory() const = 0;
    
};

} // namespace planmt
