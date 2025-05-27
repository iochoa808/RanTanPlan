# planMT

This project implements an automated planner that uses a planning-as-satisfiability approach.
It uses Unified Planning (UP) as a frontend and communicates with a C++ backend via protobuf files.

## Prerequisites

- Python 3.8+
- Unified Planning library (`pip install unified-planning[grpc]`)
- Protobuf > 29 (macOS has a breaking bug with 29)
- CMake

## Setup

### 1. Compile the C++ Planner
   - Navigate to `cpp_planner/`.
   - Place the `unified_planning.proto` file (from the [Unified Planning repository](https://github.com/aiplan4eu/unified-planning/blob/master/unified_planning/grpc/unified_planning.proto)) into the `planner/proto/` directory.
   - Build the C++ planner using cmake:
     ```bash
     cd planner
     mkdir build && cd build
     cmake ..
     make
     ```
     This will create an executable (e.g., `my_planner_cpp`) in the `planner/build/` directory.

### 2. Python UP Integration
   - Navigate to `up_integration/`.
   - Install the Python package in editable mode:
     ```bash
     cd up_integration
     pip install -e .
     ```
   This will make the planner discoverable by Unified Planning. Ensure the path to the C++ executable in `planner_wrapper.py` is correct.

## Running an Example
   - Navigate to the `examples/` directory.
   - Run the example script:
     ```bash
     cd examples
     python run_my_planner.py
     ```