cmake_minimum_required(VERSION 3.14)

# ==============================================================================
# PROJECT CONFIGURATION
# ==============================================================================
project(rantanplan)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default output directory
if(NOT DEFINED OUTPUT_DIR)
    set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../bin" CACHE STRING "Output directory for executable")
endif()

# ==============================================================================
# PROTOBUF SETUP
# ==============================================================================
option(FETCH_PROTOBUF "Force fetching protobuf via FetchContent instead of using system protobuf" OFF)

# Try system protobuf first (unless forced to fetch)
if(NOT FETCH_PROTOBUF)
    # https://github.com/protocolbuffers/protobuf/issues/12637
    if(APPLE)
        find_package(Protobuf QUIET CONFIG)
    else()
        find_package(Protobuf QUIET)
    endif()
endif()

if(Protobuf_FOUND OR PROTOBUF_FOUND)
    message(STATUS "Found system protobuf: ${Protobuf_VERSION}")
else()
    message(STATUS "System protobuf not found, fetching via FetchContent...")
    include(FetchContent)

    # Configure protobuf build options before fetching
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
    set(protobuf_ABSL_PROVIDER "module" CACHE STRING "" FORCE)
    set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
    set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG        v25.6
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES "third_party/abseil-cpp"
    )
    FetchContent_MakeAvailable(protobuf)

    # protobuf_generate is not automatically available via FetchContent,
    # include it explicitly from the fetched protobuf source
    include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)
endif()

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
add_executable(rantanplan
    src/main.cpp
    src/config/config.cpp
    src/config/cli_parser.cpp
    src/config/strategy_registry.cpp
    src/config/strategy_factory.cpp
    src/util/memory_tracker.cpp
    src/util/stats.cpp
    src/util/scoped_timer.cpp
    src/util/logger.cpp
    src/problem/real.cpp
    src/problem/atom.cpp
    src/problem/parameter.cpp
    src/problem/expr_pool.cpp
    src/problem/effect_expression.cpp
    src/problem/effect.cpp
    src/problem/object.cpp
    src/problem/fluent.cpp
    src/problem/goal.cpp
    src/problem/assignment.cpp
    src/problem/action.cpp
    src/problem/plan.cpp
    src/problem/problem.cpp
    src/problem/protobuf_io.cpp
    src/problem/type.cpp
    src/problem/visitors/fluent_collector.cpp
    src/problem/visitors/fluent_polarity_collector.cpp
    src/encoders/lifted_encoding_visitor.cpp
    src/encoders/grounded_encoding_visitor.cpp
    src/encoders/grounded_encoder.cpp
    src/encoders/chained_grounded_encoder.cpp
    src/encoders/r2e_grounded_encoder.cpp
    src/encoders/parallelism/sequential_semantics.cpp
    src/encoders/parallelism/forall_semantics.cpp
    src/encoders/parallelism/exists_semantics.cpp
    src/encoders/z3_variable_factory.cpp
    src/planners/base_planner.cpp
    src/planners/sequential.cpp
    src/planners/double_tail_planner.cpp
    src/planners/branch_and_bound_planner.cpp
    src/planners/pdla_planner.cpp
    src/planners/formula_exporter.cpp
    src/encoders/abstract_suffix_encoder.cpp
    src/planners/propagators/propagator_strategy.cpp
    src/planners/propagators/modules/logging_module.cpp
    src/planners/propagators/modules/exists_cycle_module.cpp
    src/planners/propagators/modules/forall_mutex_module.cpp
    src/planners/propagators/modules/frame_axiom_module.cpp
    src/symmetries/smt_symmetry_checker.cpp
    src/abstraction/achievers_analysis.cpp
    src/analysis/relaxed_planning_graph.cpp
    src/analysis/numeric_relaxed_planning_graph.cpp
    src/analysis/graph.cpp
    src/analysis/interference_analysis.cpp
    src/analysis/interference_analyzer.cpp
    src/analysis/eager_interference_analysis.cpp
    src/analysis/eager_semantic_interference_analysis.cpp
    src/analysis/lazy_interference_analysis.cpp
    src/analysis/semantic_interference_analysis.cpp
    src/analysis/numeric_constraint_analyzer.cpp
    src/passes/pipeline.cpp
    src/passes/numeric_rpg_pass.cpp
    src/passes/goal_relevance_pass.cpp
    src/passes/invariant_oracle_pass.cpp
    src/passes/symmetry_pass.cpp
    src/passes/grounding_pass.cpp
    src/passes/cwa_initial_state_pass.cpp
    src/passes/strategy_resolution_pass.cpp
    src/passes/interference_pass.cpp
    src/passes/static_fluent_pass.cpp
    src/problem/substitution.cpp
    src/grounding/fact_index.cpp
    src/grounding/binding_matcher.cpp
    src/grounding/action_instantiator.cpp
    src/grounding/reachability_grounder.cpp
    src/grounding/numeric_bounds_index.cpp
    src/grounding/object_fluent_index.cpp
    src/grounding/interval_evaluator.cpp
)

# Define proto files and generate C++ sources
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/unified_planning.proto)
message(STATUS "PROTO_FILES: ${PROTO_FILES}")
# generate protobuf files for the target
protobuf_generate(TARGET rantanplan
    PROTOS ${PROTO_FILES}
    LANGUAGE cpp
)

# ==============================================================================
# TARGET CONFIGURATION
# ==============================================================================
# Include directories
target_include_directories(rantanplan PRIVATE
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/abstraction/     # abstraction headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/analysis/        # analysis headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/passes/          # pass system headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/grounding/       # grounding headers
    ${Z3_INCLUDE_DIR}                                # Z3 headers
)

# Link libraries
target_link_libraries(rantanplan PRIVATE
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
    # dependency path in the 'rantanplan' executable from 'libz3.dylib'
    # to the absolute path ${Z3_LIBRARY}.
    message(STATUS "macOS manual Z3: Using install_name_tool to set absolute path for libz3.dylib in 'rantanplan'")
    add_custom_command(TARGET rantanplan POST_BUILD
        COMMAND ${CMAKE_INSTALL_NAME_TOOL} -change libz3.dylib ${Z3_LIBRARY} $<TARGET_FILE:rantanplan>
        COMMENT "Fixing Z3 dylib reference in rantanplan executable to absolute path"
        VERBATIM
    )
endif()

# ==============================================================================
# INSTALLATION
# ==============================================================================
install(TARGETS rantanplan DESTINATION ${OUTPUT_DIR})

# ==============================================================================
# Set RPATH for Z3 if found manually
# ==============================================================================
if(NOT Z3_FOUND AND Z3_LIB_DIR)
    set_target_properties(rantanplan PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "${Z3_LIB_DIR}"
        SKIP_BUILD_RPATH FALSE
        SKIP_INSTALL_RPATH FALSE
    )
endif()