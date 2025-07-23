#include "forall_propagator.h"
#include "../../encoders/z3_variable_factory.h"
#include <iostream>

namespace planmt {

ForallPropagator::ForallPropagator(z3::solver& solver, const Problem& problem)
    : z3::user_propagator_base(&solver), problem_(&problem), encoder_(nullptr), fixed_values_(solver.ctx()) {
    
    // Define callbacks for the user propagator
    register_fixed();
    
    // Initialize storage for fixed variables per scope
    fixed_cnt_.push(0);
}

/*
    def push(self):
        self.levels.append(len(self.trail))

    def pop(self, n):
        for _ in range(n):
            if self.levels:
                # Find the start of the current decision level
                level_start = self.levels.pop()
                # Undo all changes recorded after this level
                while len(self.trail) > level_start:
                    step, action = self.trail.pop()
                    self.current[step].remove(action)
        self.consistent = True
*/
void ForallPropagator::push() {
    // Z3 is entering a new backtracking scope
    fixed_cnt_.push(fixed_values_.size());
}

void ForallPropagator::pop(unsigned num_scopes) {
    // Z3 is backtracking
    for (unsigned i = 0; i < num_scopes; ++i) {
        if (!fixed_cnt_.empty()) {
            unsigned last_cnt = fixed_cnt_.top();
            fixed_cnt_.pop();
            
            // Remove fixed values that were added after this scope
            fixed_values_.resize(last_cnt);
        }
    }
}

void ForallPropagator::fixed(z3::expr const &ast, z3::expr const &value) {
    // This is called whenever Z3 fixes a variable to a value
    fixed_values_.push_back(ast);
    std::cout << "ForallPropagator::fixed() called for variable: " 
              << ast.to_string() << " with value: " << value.to_string() << std::endl;  
/*

    def _fixed(self, action, value):
        if value and self.consistent:
            # Parse action name and step
            actions = str(action).split('_')
            step = int(actions[-1])
            action_name = '_'.join(actions[:-1])
            if step >= len(self.current):
                while step >= len(self.current):
                    self.current.append(set())
                self.current[step].add(action_name)
                self.trail.append((step, action_name))
                # There cannot be any interference: no other actions in step are True
                return
            literals = set()
            self.trail.append((step, action_name))
            self.current[step].add(action_name)
            for dest in self.current[step] & set(self.graph.neighbors(action_name)):
                literals.add(self.encoder.get_action_var(dest, step))
                self.consistent = False
            for dest in set(self.graph.neighbors(action_name)) - self.current[step] - self.propagated[step]:
                if (dest, action_name) not in self.propagated[step]:
                    if dest not in self.nots[step]:
                        self.nots[step][dest] = z3.Not(self.encoder.get_action_var(dest, step))
                    self.propagate(
                        e=self.nots[step][dest],
                        ids=[action],
                        eqs=[]
                    )
                    self.propagated[step].add((dest, action_name))
                    self.propagated[step].add((action_name, dest))

            # Checking and adding in nodes using set intersection
            for source in self.current[step] & set(self.graph.predecessors(action_name)):
                literals.add(self.encoder.get_action_var(source, step))
                self.consistent = False
            for source in set(self.graph.neighbors(action_name)) - self.current[step]:
                if (source, action_name) not in self.propagated[step]:
                    if source not in self.nots[step]:
                        self.nots[step][source] = z3.Not(self.encoder.get_action_var(source, step))
                    self.propagate(
                        e=self.nots[step][source],
                        ids=[action],
                        eqs=[]
                    )
                    self.propagated[step].add((source, action_name))
                    self.propagated[step].add((action_name, source))
            # Check if anything has caused interference
            if literals:
                literals.add(action)  # New action itself is only added once
                self.conflict(deps=list(literals), eqs=[])
*/
}

z3::user_propagator_base* ForallPropagator::fresh(z3::context& ctx) {
    // For now, return null to indicate we don't support fresh instances
    // TODO: Implement proper fresh instance creation if needed
    return nullptr;
}

void ForallPropagator::initialize(z3::solver& solver, const GroundedEncoder& encoder) {
    // Store reference to encoder for variable factory access
    encoder_ = &encoder;
}

void ForallPropagator::register_timestep_variables(int timestep) {
    
    const Z3VariableFactory& var_factory = encoder_->get_variable_factory();
    // For timestep 0: register nothing as there are no actions
    if (timestep == 0) return;
    // For timestep t > 0: register action variables for t-1 
    // 1. Register action variables for timestep t-1
    if (registered_action_vars_.find(timestep - 1) == registered_action_vars_.end()) {
        auto prev_action_vars = var_factory.get_all_action_variables(timestep - 1);
        if (!prev_action_vars.empty()) {
            registered_action_vars_[timestep - 1] = std::move(prev_action_vars);
            for (const auto& var : registered_action_vars_[timestep - 1]) {
                add(var); //std::cout << "  Registered with Z3: " << var.to_string() << std::endl;
            }
        }
    }
}

} // namespace planmt
