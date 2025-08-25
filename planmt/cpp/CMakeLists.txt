cmake_minimum_required(VERSION 3.10)

# ==============================================================================
# PROJECT CONFIGURATION
# ==============================================================================
project(planmt)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default output directory
if(NOT DEFINED OUTPUT_DIR)
    set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bin" CACHE STRING "Output directory for executable")
endif()

# ==============================================================================
# PROTOBUF SETUP
# ==============================================================================
# https://github.com/protocolbuffers/protobuf/issues/12637
if(APPLE)
    find_package(Protobuf REQUIRED CONFIG)
else()
    find_package(Protobuf REQUIRED)
endif()
# Abseil (Google's C++ common libraries) for protobuf
#find_package(absl REQUIRED)
# no longer needed, protobuf now uses absl internally

# ==============================================================================
# Z3 SOLVER SETUP
# ==============================================================================
# Try to find Z3 using modern CMake first
find_package(Z3 QUIET)

if(Z3_FOUND)
    message(STATUS "Found Z3 using CMake package config")
    set(Z3_LINK_LIBRARY z3::libz3)
else()
    # Fallback to manual Z3 setup (original approach)
    message(STATUS "Using manual Z3 setup")
    
    # Z3 paths are set by setup.py during Python package installation
    set(Z3_INCLUDE_DIR ${Z3_INCLUDE_DIR} CACHE STRING "Z3 include directory")
    set(Z3_LIB_DIR ${Z3_LIB_DIR} CACHE STRING "Z3 library directory")

    # Locate Z3 library
    find_library(Z3_LIBRARY
        NAMES z3 libz3
        PATHS ${Z3_LIB_DIR}
        NO_DEFAULT_PATH
    )

    if(NOT Z3_LIBRARY)
        message(FATAL_ERROR "Z3 library not found in ${Z3_LIB_DIR}")
    endif()

    message(STATUS "Found Z3 library: ${Z3_LIBRARY}")
    set(Z3_LINK_LIBRARY ${Z3_LIBRARY}) # Use the full path to the library
endif()


# ==============================================================================
# TARGET DEFINITION
# ==============================================================================
# Create main executable
add_executable(planmt
    src/main.cpp
    src/config/config.cpp
    src/config/cli_parser.cpp
    src/util/memory_tracker.cpp
    src/util/stats.cpp
    src/problem/real.cpp
    src/problem/atom.cpp
    src/problem/parameter.cpp
    src/problem/expression.cpp
    src/problem/effect_expression.cpp
    src/problem/effect.cpp
    src/problem/object.cpp
    src/problem/fluent.cpp
    src/problem/goal.cpp
    src/problem/assignment.cpp
    src/problem/action.cpp
    src/problem/plan.cpp
    src/problem/problem.cpp
    src/problem/type.cpp
    src/problem/visitors/expression_visitor.cpp
    src/encoders/lifted_encoding_visitor.cpp
    src/encoders/grounded_encoding_visitor.cpp
    src/encoders/grounded_encoder.cpp
    src/encoders/chained_grounded_encoder.cpp
    src/encoders/r2e_grounded_encoder.cpp
    src/encoders/reified_grounded_encoder.cpp
    src/encoders/encoder_factory.cpp
    src/encoders/parallelism/interference_analyzer.cpp
    src/encoders/parallelism/interference_analysis.cpp
    src/encoders/parallelism/eager_interference_analysis.cpp
    src/encoders/parallelism/lazy_interference_analysis.cpp
    src/encoders/parallelism/semantic_interference_analysis.cpp
    src/encoders/parallelism/interference_analysis_factory.cpp
    src/encoders/parallelism/sequential_semantics.cpp
    src/encoders/parallelism/forall_semantics.cpp
    src/encoders/parallelism/exists_semantics.cpp
    src/encoders/parallelism/parallelism_factory.cpp
    src/encoders/parallelism/graph.cpp
    src/encoders/z3_variable_factory.cpp
    src/planners/sequential.cpp
    src/planners/propagators/forall_propagator.cpp
    src/planners/propagators/lazy_forall_propagator.cpp
    src/planners/propagators/exists_propagator.cpp
    src/planners/propagators/decision_heuristic_propagator.cpp
    src/planners/propagators/propagator_factory.cpp
    src/symmetries/smt_symmetry_checker.cpp
    src/arpg/supporter.cpp
    src/arpg/arpg.cpp
)

# Define proto files and generate C++ sources
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/unified_planning.proto)
message(STATUS "PROTO_FILES: ${PROTO_FILES}")
# generate protobuf files for the target
protobuf_generate(TARGET planmt
    PROTOS ${PROTO_FILES}
    LANGUAGE cpp
)

# ==============================================================================
# TARGET CONFIGURATION
# ==============================================================================
# Include directories
target_include_directories(planmt PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}                      # generated protobuf sources
    ${CMAKE_CURRENT_BINARY_DIR}/proto                # generated protobuf headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src                  # our source headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config           # config headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util             # utility headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/problem          # problem representation headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/problem/visitors # visitor headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoders/        # encoders headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/planners/        # planners headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/symmetries/      # symmetries headers
    ${Protobuf_INCLUDE_DIRS}                         # protobuf lib headers
    ${Z3_INCLUDE_DIR}                                # Z3 headers
)

# Link libraries
target_link_libraries(planmt PRIVATE
    protobuf::libprotobuf
    ${Z3_LINK_LIBRARY} # Ensure this uses the full path from Z3_LIBRARY
)

# ==============================================================================
# PLATFORM-SPECIFIC FIXES
# ==============================================================================
# Fix Z3 library reference on macOS
if(APPLE AND NOT Z3_FOUND)
    # This applies only when Z3 is found manually (NOT Z3_FOUND is true).
    # Z3_LIBRARY holds the full path to libz3.dylib.
    # Z3_LIB_DIR holds the directory containing libz3.dylib.

    # Instead of relying on RPATH, use install_name_tool to change the recorded
    # dependency path in the 'planmt' executable from 'libz3.dylib'
    # to the absolute path ${Z3_LIBRARY}.
    message(STATUS "macOS manual Z3: Using install_name_tool to set absolute path for libz3.dylib in 'planmt'")
    add_custom_command(TARGET planmt POST_BUILD
        COMMAND ${CMAKE_INSTALL_NAME_TOOL} -change libz3.dylib ${Z3_LIBRARY} $<TARGET_FILE:planmt>
        COMMENT "Fixing Z3 dylib reference in planmt executable to absolute path"
        VERBATIM
    )
endif()

# ==============================================================================
# INSTALLATION
# ==============================================================================
install(TARGETS planmt DESTINATION ${OUTPUT_DIR})

# ==============================================================================
# Set RPATH for Z3 if found manually
# ==============================================================================
if(NOT Z3_FOUND AND Z3_LIB_DIR)
    set_target_properties(planmt PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "${Z3_LIB_DIR}"
        SKIP_BUILD_RPATH FALSE
        SKIP_INSTALL_RPATH FALSE
    )
endif()