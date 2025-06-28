# planMT

A high-performance automated planner that uses a planning-as-satisfiability approach

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         planMT Package                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐              ┌─────────────────────────┐   │
│  │   CLI Interface │              │   Library Interface     │   │
│  │   (planmt cmd)  │              │   (Unified Planning)    │   │
│  │                 │              │                         │   │
│  │• argparse       │              │ • UP Engine Plugin      │   │
│  │• File validation│              │ • planMTPlanner class   │   │
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
│  │              C++ planMT Engine                          │    │
│  │                                                         │    │
│  │ • SAT-based planning                                    │    │
│  │ • PDDL parsing                                          │    │
│  │ • Optimization                                          │    │
│  │ • Executable: planmt/bin/planmt                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

Input: PDDL Domain + Problem → Processing → Output: Plan or UNSAT
```

## Features

- **Easy Installation**: Install as a Python package with `pip install`
- **CLI Interface**: Run from command line with `planmt -d domain.pddl -p problem.pddl`
- **High Performance**: C++ backend for efficient SAT-based planning

## Installation

### Prerequisites

- Python 3.8+
- CMake (for building the C++ planner)
- Git
- protobuf libraries

### Install from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/pyPMT/planMT.git
   cd planMT
   ```

2. Install the package:
   ```bash
   pip install -e .
   python build.py # build the C++ planner
   ```

## Usage

### Command Line Interface

Once installed, you can use planMT from anywhere:

```bash
# Basic usage
planmt --domain examples/domain.pddl --problem examples/problem.pddl

# Save plan to file
planmt -d domain.pddl -p problem.pddl --output-plan solution.txt

# Show help
planmt --help
```

### Library Usage (Unified Planning)

```python
import unified_planning as up
from unified_planning.shortcuts import *

# Create a planning problem
problem = Problem()
# ... define your problem ...

# planMT is automatically available as an engine
with OneshotPlanner(name='planMT') as planner:
    result = planner.solve(problem)
    if result.status == up.engines.PlanGenerationResultStatus.SOLVED_SATISFICING:
        print(f"Found plan: {result.plan}")
```

### Direct Library Usage

```python
from planmt.planner_wrapper import planMTPlanner

# Create planner instance
planner = planMTPlanner()

# Solve a problem
result = planner.solve(problem)
```

## Citation

If you use planMT in your research, please cite:

```bibtex
@software{planmt,
  title={planMT: A Planning-as-Satisfiability Planner},
  author={Joan Espasa Arxer},
  year={2025},
  url={https://github.com/pyPMT/planMT}
}
```
