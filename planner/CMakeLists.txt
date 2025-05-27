cmake_minimum_required(VERSION 3.10)
project(planmt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Protobuf
find_package(Protobuf REQUIRED)

# Specify the location of .proto files
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/unified_planning.proto)

# Generate C++ sources from .proto files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Add the executable
add_executable(planmt
    src/main.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

# Link with Protobuf libraries - use modern target
target_link_libraries(planmt PRIVATE protobuf::libprotobuf)

# Also try to link with required dependencies
find_package(absl QUIET)
if(absl_FOUND)
    target_link_libraries(planmt PRIVATE absl::base absl::log absl::check)
endif()

# Ensure generated headers can be found
target_include_directories(planmt PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} # For generated headers
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Protobuf_INCLUDE_DIRS} # Add protobuf include directories
)

# Install the executable (optional, but useful for MANIFEST.in)
install(TARGETS planmt DESTINATION bin)
set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/install)