# RantanPlan

A SAT/SMT-based automated planner with parallel action semantics. Combines a Python frontend with a high-performance C++ backend using Z3.

## Key Features

- **Three parallelism semantics**: Sequential, ∀-step (forall), ∃-step (exists)
- **Multiple encoding strategies**: Grounded, chained, R2∃-step
- **Interference analysis**: Syntactic (eager/lazy) and semantic
- **Symmetry breaking**: Object symmetry detection
- **Unified Planning integration**: Plug-and-play with UP framework

**Limitation**: Does not support PDDL actions that both delete and add the same fluent.

## Installation

**Prerequisites**: Python 3.8+, CMake, protobuf libraries (`brew install protobuf` or `apt-get install libprotobuf-dev protobuf-compiler`)

```bash
git clone https://github.com/pyPMT/RantanPlan.git
cd RantanPlan
python -m venv .venv
source .venv/bin/activate  # Always activate before build/run
pip install -e .
python build.py
```

## Usage

### Command Line

```bash
# Sequential planning
rantanplan -d domain.pddl -p problem.pddl --strategy seq

# Forall-step with lazy semantic interference and chaining
rantanplan -d domain.pddl -p problem.pddl --strategy forall-lazy-semantic-chain --timeout 120

# Exists-step with decision heuristics
rantanplan -d domain.pddl -p problem.pddl --strategy dec --detect-symmetries

# R2E semantics (declaration-order parallelism)
rantanplan -d domain.pddl -p problem.pddl --strategy r2e --output-plan solution.txt
```

### Available Strategies

| Strategy | Parallelism | Interference | Encoding | Notes |
|----------|-------------|--------------|----------|-------|
| `seq` | Sequential | N/A | Grounded | Classic planning |
| `forall` | ∀-step | Eager syntactic | Grounded | Basic parallel |
| `forall-prop` | ∀-step | Eager + propagator | Grounded | Optimized |
| `forall-lazy` | ∀-step | Lazy syntactic | Grounded | On-demand |
| `forall-lazy-semantic` | ∀-step | Lazy semantic | Grounded | Advanced |
| `forall-lazy-semantic-chain` | ∀-step | Lazy semantic | Chained | Best for ∀-step |
| `exists` | ∃-step | Eager syntactic | Grounded | Basic parallel |
| `exists-lazy` | ∃-step | Lazy syntactic | Grounded | With cycle detection |
| `exists-lazy-semantic` | ∃-step | Lazy semantic | Grounded | Advanced |
| `exists-lazy-semantic-chain` | ∃-step | Lazy semantic | Chained | Best for ∃-step |
| `r2e` | R2∃-step | Declaration order | R2E | Novel encoding |
| `dec` | ∃-step | Lazy semantic | Chained | With heuristics |

### Python API

```python
from unified_planning.shortcuts import *

problem = Problem()
# ... define problem ...

# Unified Planning integration (auto-discovered)
with OneshotPlanner(name='RantanPlan') as planner:
    result = planner.solve(problem)
    print(result.plan)

# Or use directly
from rantanplan.planner_wrapper import RantanPlanPlanner
planner = RantanPlanPlanner(strategy='forall-lazy-semantic-chain')
result = planner.solve(problem)
```

## Architecture

```
Python Frontend              C++ Backend
┌─────────────────┐         ┌──────────────────┐
│ CLI / UP Engine │ ───┐    │  Strategy Config │
│                 │    │    │  ┌──────────────┐│
│ • PDDL parsing  │    └───▶│  │ Encoder      ││
│ • CNF compiler  │ protobuf│  │ Parallelism  ││
│ • Plan validate │◀────┘   │  │ Propagator   ││
└─────────────────┘         │  │ Symmetries   ││
                            │  └──────────────┘│
                            │                  │
                            │  Z3 SMT Solver   │
                            └──────────────────┘
```

The Python frontend uses Unified Planning to parse PDDL, applies optional CNF normalization, and communicates via protobuf with the C++ backend. The backend encodes the problem as SMT, solves with Z3 using custom propagators, and returns a plan.

## Development

```bash
source .venv/bin/activate  # Always required

# Building
python build.py              # Standard release build
python build.py --clean      # Clean rebuild
python build.py --build-type debug  # Debug build

# Testing
python test.py               # All strategies, all test domains
python test.py --quick       # Fast subset (zenotravel, rover, gripper, hydropower)
python test.py -v            # Verbose output
```

## Citation

If you use RantanPlan in your research, please cite:

```bibtex
@software{rantanplan,
  title={RantanPlan: A Planning-as-SMT Planner},
  author={Joan Espasa Arxer},
  year={2025},
  url={https://github.com/pyPMT/RantanPlan}
}
```
