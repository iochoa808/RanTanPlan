# RantanPlan

A SAT/SMT-based automated planner with parallel action semantics. Combines a Python frontend with a high-performance C++ backend using Z3.

## Key Features

- **Three parallelism semantics**: Sequential, ∀-step (forall), ∃-step (exists), R2∃-step
- **Multiple encoding strategies**: Grounded, chained, R2∃-step, reified
- **Search modes**: Satisficing, cost-optimal (Branch & Bound), anytime
- **Interference analysis**: Syntactic (eager/lazy) and semantic (eager/lazy)
- **Custom Z3 propagators**: Lazy interference via cycle detection, decision heuristics
- **Numeric planning**: Full support for numeric fluents with SMT-based relaxed planning graph
- **Symmetry breaking**: Object symmetry detection and breaking
- **Preprocessing pipeline**: Reachability grounding, RPG-based action removal, CWA completion
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
# Sequential planning (satisficing by default)
rantanplan -d domain.pddl -p problem.pddl --strategy seq

# Forall-step with lazy semantic interference and chaining
rantanplan -d domain.pddl -p problem.pddl --strategy forall-lazy-semantic-chain --timeout 120

# Cost-optimal planning with Branch & Bound
rantanplan -d domain.pddl -p problem.pddl --strategy exists-lazy-semantic-chain --mode optimal

# Anytime planning (writes improving plans to disk)
rantanplan -d domain.pddl -p problem.pddl --strategy forall-lazy --mode anytime

# R2E semantics (declaration-order parallelism)
rantanplan -d domain.pddl -p problem.pddl --strategy r2e --output-plan solution.txt

# Double-tail bidirectional search
rantanplan -d domain.pddl -p problem.pddl --strategy forall-lazy-semantic-chain-dt

# Core-guided lazy activation over exists-step encoding
rantanplan -d domain.pddl -p problem.pddl --strategy causal-exists --detect-symmetries
```

### Search Modes

| Mode | Description |
|------|-------------|
| `satisficing` | (default) Find first valid plan, return immediately |
| `optimal` | Cost-optimal planning using Branch & Bound with abstract suffix. Returns `SOLVED_OPTIMALLY` when optimality is proven |
| `anytime` | Continuously find improving plans, writing each to disk as `plan.txt.1`, `.2`, etc. |

### Available Strategies

| Strategy | Parallelism | Interference | Encoding | Notes |
|----------|-------------|--------------|----------|-------|
| `seq` | Sequential | N/A | Grounded | Classic planning |
| `forall` | ∀-step | Eager syntactic | Grounded | Basic parallel |
| `forall-prop` | ∀-step | Eager + propagator | Grounded | Optimized |
| `forall-lazy` | ∀-step | Lazy syntactic | Grounded | On-demand |
| `forall-lazy-semantic` | ∀-step | Lazy semantic | Grounded | Advanced |
| `forall-lazy-semantic-chain` | ∀-step | Lazy semantic | Chained | Best for ∀-step |
| `forall-eager-semantic` | ∀-step | Eager semantic | Grounded | Experimental |
| `forall-eager-semantic-chain` | ∀-step | Eager semantic | Chained | Experimental |
| `exists` | ∃-step | Eager syntactic | Grounded | Basic parallel |
| `exists-lazy` | ∃-step | Lazy syntactic | Grounded | With cycle detection |
| `exists-lazy-semantic` | ∃-step | Lazy semantic | Grounded | Advanced |
| `exists-lazy-semantic-chain` | ∃-step | Lazy semantic | Chained | Best for ∃-step |
| `exists-eager-semantic` | ∃-step | Eager semantic | Grounded | Experimental |
| `exists-eager-semantic-chain` | ∃-step | Eager semantic | Chained | Experimental |
| `r2e` | R2∃-step | ARPG-based order | R2E | Novel encoding |

All strategies (except `-dt` variants) support `--mode optimal` and `--mode anytime` for cost-optimal and anytime planning respectively.

**Double-tail variants** (`seq-dt`, `forall-dt`, `forall-lazy-dt`, `exists-dt`, `exists-lazy-dt`, etc.) use bidirectional (forward + backward) search for faster satisficing planning. Only compatible with `--mode satisficing`.

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
┌─────────────────┐         ┌──────────────────────┐
│ CLI / UP Engine │ ───┐    │  Preprocessing       │
│                 │    │    │  ┌──────────────────┐ │
│ • PDDL parsing  │    └───▶│  │ Grounding        │ │
│ • CNF compiler  │ protobuf│  │ RPG analysis     │ │
│ • Plan validate │◀────┘   │  │ Symmetry detect  │ │
└─────────────────┘         │  │ Interference     │ │
                            │  └──────────────────┘ │
                            │  Solving               │
                            │  ┌──────────────────┐ │
                            │  │ Encoder          │ │
                            │  │ Parallelism      │ │
                            │  │ Z3 Propagators   │ │
                            │  │ B&B / Sequential │ │
                            │  └──────────────────┘ │
                            │                        │
                            │  Z3 SMT Solver         │
                            └──────────────────────┘
```

The Python frontend uses Unified Planning to parse PDDL, applies optional CNF normalization, and communicates via protobuf with the C++ backend. The backend runs a preprocessing pipeline (reachability grounding, RPG-based action removal, symmetry detection, interference analysis), then encodes the problem as SMT and solves with Z3 using custom propagators.

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
@article{Bofill_Espasa_Villaret_2016,
  title={The RANTANPLAN planner: system description},
  volume={31},
  DOI={10.1017/S0269888916000229},
  number={5},
  journal={The Knowledge Engineering Review},
  author={Bofill, Miquel and Espasa, Joan and Villaret, Mateu},
  year={2016},
  pages={452–464}
}
```
