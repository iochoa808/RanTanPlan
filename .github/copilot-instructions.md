# General rules 

You should provide clear, concise, and accurate responses to questions and requests related to the project.
You should not make assumptions about the project structure or requirements unless explicitly stated.
You should always ask for clarification if the request is ambiguous or if you are unsure about the requirements.
You should follow best practices in software development, including code quality and documentation.
You should not add any new features, tests or make drastic changes to the codebase without explicit instructions from the user.

# Project Guidelines

You should follow the project's coding standards and conventions.
You should strive to write code that is consistent with the existing codebase.
You should write code that is simple, efficient, maintainable, and easy to understand.
You should ensure that any code you write is minimally documented.
You should ensure that any changes you make do not break existing functionality.
You should always ensure your code works as expected.

# Role and Responsibilities

You are an AI assistant designed to help with software development tasks, specifically in Python and C++.
If any instruction is unclear or there is a clear choice to be made, ask for clarification before proceeding with the task.

# Project Overview

The project is a planning module that uses the Z3 theorem prover to solve planning problems defined using the Unified Planning Library in Python.
It provides a Python interface to a C++ library that implements various planning algorithms and integrates with the
Z3 solver for efficient reasoning about planning problems.

It is designed to be used as a command-line interface tool and can also be integrated into other Python applications as a library.
The project uses protobuf for communication between the Python and C++ components, allowing for efficient serialization of
the problem data structures to be processed by the C++ planning engine and then return the results back to Python for further processing or display.

# Project Structure

The C++ code is built using CMake and is wrapped for use in Python.

The project is structured as follows:
- `planmt/`: Main Python package directory
  - `cli.py`: Command-line interface implementation
  - `planner_wrapper.py`: Python wrapper that interfaces with C++ backend
  - `cpp/`: C++ source code directory
    - `src/`: C++ implementation files
      - `main.cpp`: Main entry point for C++ executable
      - `problem/`: Problem representation classes 
      - `encoders/`: SMT encoding classes 
      - `planners/`: Planning algorithm implementations 
    - `proto/`: Protobuf message definitions
    - `CMakeLists.txt`: CMake build configuration
- `pddl/`: Test PDDL domain and problem files
- `build.py`: Python script to manage CMake build process
- `setup.py`: Python package setup

## Python-C++ Communication Flow
1. Python frontend uses Unified Planning library to parse PDDL
2. Problem is serialized to protobuf format
3. C++ executable is called with protobuf input/output files
4. C++ deserializes problem, runs planner, serializes result
5. Python reads result and converts back to Unified Planning format

## Building

The project has a virtual environment that is used to build and run the project.
To activate the virtual environment, you can run the following command:
```bash
source .venv/bin/activate
```

The build system uses cmake, but needs some variables to find the Z3 libraries for it to work correctly.
Therefore, from the root of the project, you can run the following command to build the project: `python build.py`
The `python build.py --clean` command cleans the build and builds from scratch.

## Installation and Testing
The python module should be installed in edit mode for it to work correctly:
```bash
pip install -e .
```

Once installed, a CLI interface is available as the "planmt" command.
To test the project, you can run the following: 
```bash
planmt -v -d pddl/zenotravel/domain.pddl -p pddl/zenotravel/problem.pddl
```

Other test domains available:
- `pddl/rover/` - Mars rover planning
- `pddl/satellite/` - Satellite observation scheduling  
- `pddl/scanalyzer-3d-sequential-optimal-strips/` - Assembly line optimization