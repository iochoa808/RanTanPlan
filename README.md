# planMT

A high-performance automated planner that uses a planning-as-satisfiability approach, implemented as a proper Python package with both C├── examples/                 # Example PDDL files
├── pyproject.toml           # Package configuration & build system
└── README.md               # This filend library interfaces. It integrates with Unified Planning (UP) and uses a C++ backend for efficient solving.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         planMT Package                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────┐              ┌─────────────────────────┐   │
│  │   CLI Interface │              │   Library Interface    │   │
│  │   (planmt cmd)  │              │   (Unified Planning)   │   │
│  │                 │              │                         │   │
│  │ • argparse      │              │ • UP Engine Plugin     │   │
│  │ • File validation│              │ • planMTPlanner class │   │
│  │ • Result format │              │ • Auto-discovery      │   │
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
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              C++ planMT Engine                          │   │
│  │                                                         │   │
│  │ • SAT-based planning                                    │   │
│  │ • PDDL parsing                                          │   │
│  │ • Optimization                                          │   │
│  │ • Executable: planmt/bin/planmt                         │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

Input: PDDL Domain + Problem → Processing → Output: Plan or UNSAT
```

## Features

- **🔧 Easy Installation**: Install as a Python package with `pip install`
- **🖥️ CLI Interface**: Run from command line with `planmt --domain domain.pddl --problem problem.pddl`
- **📚 Library Integration**: Use as a Unified Planning engine in Python code
- **⚡ High Performance**: C++ backend for efficient SAT-based planning
- **🔄 Auto-compilation**: C++ planner builds automatically during installation
- **🎯 Auto-discovery**: Automatically detected by Unified Planning framework

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

2. Install the package (this will automatically compile the C++ planner):
   ```bash
   pip install -e .
   ```

The installation process will:
- Install Python dependencies (unified-planning, protobuf)
- Compile the C++ planner using CMake
- Set up the CLI command `planmt`
- Register the planner with Unified Planning

## Usage

### Command Line Interface

Once installed, you can use planMT from anywhere:

```bash
# Basic usage
planmt --domain examples/domain.pddl --problem examples/problem.pddl

# With verbose output and custom timeout
planmt -d domain.pddl -p problem.pddl --verbose --timeout 60

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

## Project Structure

```
planMT/
├── planmt/                    # Main Python package
│   ├── __init__.py           # Package initialization
│   ├── cli.py                # Command-line interface
│   ├── planner_wrapper.py    # UP engine wrapper
│   ├── bin/                  # Compiled executables
│   │   └── planmt            # C++ planner executable
│   └── cpp/                  # C++ source code
│       ├── src/main.cpp      # Main C++ implementation
│       ├── proto/            # Protocol buffer definitions
│       └── CMakeLists.txt    # Build configuration
├── examples/                 # Example PDDL files
├── pyproject.toml           # Package configuration & build system
└── README.md               # This file
```

## CLI Options

```
usage: planmt [-h] -d DOMAIN -p PROBLEM [--timeout TIMEOUT] 
              [--executable EXECUTABLE] [-v] [-q] [--output-plan OUTPUT_PLAN] 
              [--version]

Arguments:
  -d, --domain DOMAIN          Path to the PDDL domain file
  -p, --problem PROBLEM        Path to the PDDL problem file
  --timeout TIMEOUT           Timeout in seconds (default: 30.0)
  --executable EXECUTABLE     Custom path to planMT executable
  -v, --verbose               Enable verbose output
  -q, --quiet                 Suppress planner output
  --output-plan OUTPUT_PLAN   Save plan to file
  --version                   Show version information
```

## Development

### Building from Source

1. Clone and navigate to the repository:
   ```bash
   git clone https://github.com/pyPMT/planMT.git
   cd planMT
   ```

2. Create a virtual environment:
   ```bash
   python -m venv .venv
   source .venv/bin/activate
   ```

3. Install in development mode:
   ```bash
   pip install -e .
   ```

### Manual C++ Compilation

If you need to rebuild the C++ planner manually:

```bash
cd planmt/cpp
mkdir -p build && cd build
cmake ..
make
make install  # Copies executable to planmt/bin/
```

### Testing

Run the planner with the included examples:

```bash
# Test CLI
planmt --domain examples/domain.pddl --problem examples/problem.pddl --verbose

# Test library integration
python examples/run_planner.py
```

## Troubleshooting

### Common Issues

1. **Command not found: planmt**
   - Ensure you're in the correct virtual environment
   - Reinstall with `pip install -e .`

2. **C++ compilation errors**
   - Ensure CMake is installed: `brew install cmake` (macOS) or `apt-get install cmake` (Linux)
   - Check that all dependencies are available

3. **Protobuf version conflicts**
   - Ensure protobuf > 29: `pip install "protobuf>=5.30"`
   - On macOS, avoid protobuf version 29 due to known issues

4. **Executable not found**
   - The planner automatically detects the executable location
   - Use `--executable /path/to/planmt` to specify manually

### Verbose Output

Use `--verbose` flag to see detailed execution information:

```bash
planmt -d domain.pddl -p problem.pddl --verbose
```

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Make your changes and test them
4. Submit a pull request

### Code Style

- Follow PEP 8 for Python code
- Use clear, descriptive variable names
- Add docstrings to public functions
- Include type hints where appropriate

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

## Acknowledgments

- [Unified Planning](https://github.com/aiplan4eu/unified-planning) framework
- Planning-as-satisfiability research community
- Protocol Buffers for efficient communication

## Links

- **Homepage**: https://github.com/pyPMT/planMT
- **Documentation**: https://github.com/pyPMT/planMT/wiki
- **Issue Tracker**: https://github.com/pyPMT/planMT/issues
- **Unified Planning**: https://github.com/aiplan4eu/unified-planning