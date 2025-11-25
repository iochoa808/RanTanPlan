# RantanPlan

An automated planning system that uses a planning-as-SMT approach. It combines a Python frontend with a C++ backend for SMT-based planning.

Supports three parallelism strategies (sequential, forall, exists) with optimization configurations and interference analysis.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         RantanPlan Package                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐              ┌─────────────────────────┐   │
│  │   CLI Interface │              │   Library Interface     │   │
│  │   (rantanplan cmd)  │              │   (Unified Planning)    │   │
│  │                 │              │                         │   │
│  │• argparse       │              │ • UP Engine Plugin      │   │
│  │• File validation│              │ • RantanPlanPlanner class   │   │
│  │• Result format  │              │ • Auto-discovery        │   │
│  └─────────┬───────┘              └─────────┬───────────────┘   │
│            │                                │                   │
│            └──────┬─────────────────────────┘                   │
│                   │                                             │
│           ┌───────▼─────────┐                                   │
│           │ planner_wrapper │                                   │
│           │                 │                                   │
│           │ • Process mgmt  │                                   │
│           │ • Protobuf I/O  │                                   │
│           │ • Error handling│                                   │
│           └───────┬─────────┘                                   │
│                   │                                             │
├───────────────────┼─────────────────────────────────────────────┤
│                   │ protobuf                                    │
│                   ▼                                             │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │              C++ RantanPlan Engine                          │    │
│  │                                                         │    │
│  │ • SMT-based planning                                    │    │
│  │ • PDDL parsing                                          │    │
│  │ • Optimization                                          │    │
│  │ • Executable: rantanplan/bin/rantanplan                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

Input: PDDL Domain + Problem → Processing → Output: Plan or UNSAT
```

## Features

- Install as a Python package
- Command line interface with strategy configuration
- C++ backend for SMT-based planning
- Three parallelism strategies:
  - `sequential`: One action per timestep
  - `forall`: Parallel actions if none interfere
  - `exists`: Actions execute if non-interfering order exists
- Three interference analysis modes:
  - `eager`: Pre-computed syntactic interferences
  - `lazy`: On-demand interference computation
  - `semantic`: Advanced semantic interference analysis
- Multiple encoding options:
  - `grounded`: Standard SMT encoding
  - `chained`: Chained encoding for cumulative effects
  - `r2e`: R2∃-step semantics with built-in parallelism
- Object symmetry detection and breaking
- Unified Planning library integration

**Note:** RantanPlan does not support PDDL delete-then-set effect semantics. Actions that both delete and add the same fluent will be treated as contradictory and may cause planning failures.

## Installation

### Prerequisites

- Python 3.8+
- CMake (for building the C++ planner)
- Git
- **protobuf libraries** (external dependency - install via system package manager)
  - Ubuntu/Debian: `sudo apt-get install libprotobuf-dev protobuf-compiler`
  - macOS: `brew install protobuf`
  - Other systems: See [protobuf installation guide](https://protobuf.dev/downloads/)

### Install from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/pyPMT/RantanPlan.git
   cd RantanPlan
   ```

2. Create and activate a virtual environment:
   ```bash
   python -m venv .venv
   source .venv/bin/activate  # Required for all build/run operations
   ```

3. Install the package:
   ```bash
   pip install -e .          # Install in development mode
   python build.py           # Build the C++ planner
   ```

## Usage

### Command Line Interface

Basic usage:

```bash
# Basic usage with sequential strategy
rantanplan -d pddl/test/zenotravel/domain.pddl -p pddl/test/zenotravel/problem.pddl --strategy sequential

# Use optimized forall strategy with timeout
rantanplan -d domain.pddl -p problem.pddl --strategy forall-optimized --timeout 120

# Semantic interference analysis with chained encoding
rantanplan -d domain.pddl -p problem.pddl --strategy forall-lazy-semantic-chained

# R2E semantics for built-in parallelism
rantanplan -d domain.pddl -p problem.pddl --strategy r2e

# Enable symmetry detection
rantanplan -d domain.pddl -p problem.pddl --strategy forall-optimized --detect-symmetries

# Save plan to file
rantanplan -d domain.pddl -p problem.pddl --strategy exists-optimized-semantic --output-plan solution.txt

# Show help
rantanplan --help
```

#### Available Strategies

**Basic:**
- `sequential`: Sequential encoding
- `forall-basic`: Forall semantics with pre-computed interference
- `exists-basic`: Exists semantics with pre-computed interference

**Optimized:**
- `forall-optimized`: Forall with propagation
- `forall-lazy`: Forall with lazy interference analysis
- `exists-optimized`: Exists with lazy interference and cycle detection

**Advanced (Semantic Interference):**
- `forall-lazy-semantic`: Forall with semantic interference analysis
- `exists-optimized-semantic`: Exists with semantic interference analysis
- `forall-lazy-semantic-chained`: Forall with semantic analysis and chained encoding
- `exists-optimized-semantic-chained`: Exists with semantic analysis and chained encoding

**Novel Encodings:**
- `r2e`: R2∃-step semantics with declaration-order parallelism

### Library Usage (Unified Planning)

```python
import unified_planning as up
from unified_planning.shortcuts import *

# Create a planning problem
problem = Problem()
# ... define your problem ...

# RantanPlan is automatically available as an engine
with OneshotPlanner(name='RantanPlan') as planner:
    result = planner.solve(problem)
    if result.status == up.engines.PlanGenerationResultStatus.SOLVED_SATISFICING:
        print(f"Found plan: {result.plan}")
```

### Direct Library Usage

```python
from rantanplan.planner_wrapper import RantanPlanPlanner

# Create planner instance
planner = RantanPlanPlanner()

# Solve a problem
result = planner.solve(problem)
```

## Testing and Development

### Running Tests

```bash
# Activate virtual environment first
source .venv/bin/activate

# Run comprehensive tests (all strategies, all domains)
python test.py

# Quick test subset for development
python test.py --quick

# Verbose test output
python test.py -v
```

### Building and Cleaning

```bash
# Standard build
python build.py

# Clean build
python build.py --clean

# Debug build
python build.py --build-type debug
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
