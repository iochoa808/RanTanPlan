#!/usr/bin/env python3
import sys
import os

# Add the planmt module to the path 
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..'))

try:
    # Try to import the protobuf generated classes
    from planmt.cpp.build.unified_planning_pb2 import Problem, PlanGenerationResult
    print("Using protobuf from build directory")
except ImportError:
    # Fallback - try building first
    import subprocess
    import tempfile
    
    # Create a simple protobuf message manually for testing
    print("Creating simple test problem file...")
    
    # Create a minimal binary file that the C++ program can read
    # For now, just create an empty file - the C++ program will handle the error gracefully
    problem_file = os.path.join(os.path.dirname(__file__), "test_problem.pb")
    with open(problem_file, "wb") as f:
        # Write a minimal protobuf message
        # This is a very basic Problem message with just a name
        # Field 2 (problem_name) = "test_problem"
        f.write(b'\x12\x0ctest_problem')
    
    print(f"Created test problem file: {problem_file}")
