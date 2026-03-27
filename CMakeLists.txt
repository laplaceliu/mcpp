cmake_minimum_required(VERSION 3.10)
project(mcpp VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(MCPP_BUILD_TESTS "Build tests" ON)
option(MCPP_BUILD_EXAMPLES "Build examples" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Dependencies directories
set(DEPS_PREFIX ${CMAKE_SOURCE_DIR}/_deps)
set(DEPS_INSTALL_PREFIX ${DEPS_PREFIX}/install)
set(DEPS_SRC_PREFIX ${DEPS_PREFIX}/src)

# Find nlohmann/json
find_package(nlohmann_json 3.10 REQUIRED PATHS ${DEPS_INSTALL_PREFIX})

# cpp-httplib (header-only library)
set(HTTPLIB_INCLUDE_DIR ${DEPS_SRC_PREFIX}/cpp-httplib-src)
if(NOT EXISTS ${HTTPLIB_INCLUDE_DIR}/httplib.h)
    message(FATAL_ERROR "cpp-httplib not found at ${HTTPLIB_INCLUDE_DIR}")
endif()

# OpenSSL (required for WebSocket)
find_package(OpenSSL REQUIRED)

# GoogleTest - pre-built libraries
set(GTEST_ROOT ${DEPS_PREFIX})
set(GTEST_INCLUDE_DIRS
    ${DEPS_SRC_PREFIX}/googletest-src/googletest/include
    ${DEPS_SRC_PREFIX}/googletest-src/googlemock/include
)
set(GTEST_BREAK_SEVERITY_THRESHOLD "" CACHE STRING "")
set(GTEST_HAS_PTHREAD OFF)

# Create imported targets for GTest
add_library(GTest::gtest STATIC IMPORTED)
set_target_properties(GTest::gtest PROPERTIES
    IMPORTED_LOCATION ${DEPS_PREFIX}/lib/libgtest.a
    INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIRS}"
)

add_library(GTest::gtest_main STATIC IMPORTED)
set_target_properties(GTest::gtest_main PROPERTIES
    IMPORTED_LOCATION ${DEPS_PREFIX}/lib/libgtest_main.a
)

add_library(GTest::gmock STATIC IMPORTED)
set_target_properties(GTest::gmock PROPERTIES
    IMPORTED_LOCATION ${DEPS_PREFIX}/lib/libgmock.a
    INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIRS}"
)

add_library(GTest::gmock_main STATIC IMPORTED)
set_target_properties(GTest::gmock_main PROPERTIES
    IMPORTED_LOCATION ${DEPS_PREFIX}/lib/libgmock_main.a
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${DEPS_INSTALL_PREFIX}/include
    ${HTTPLIB_INCLUDE_DIR}
)

# Source files
set(MCPP_CORE_SOURCES
    src/core/json.cpp
    src/core/error.cpp
)

set(MCPP_PROTOCOL_SOURCES
    src/protocol/message.cpp
    src/protocol/request.cpp
    src/protocol/response.cpp
)

set(MCPP_TRANSPORT_SOURCES
    src/transport/transport.cpp
    src/transport/stdio.cpp
    src/transport/http.cpp
    src/transport/websocket.cpp
)

set(MCPP_SERVER_SOURCES
    src/server/server.cpp
    src/server/tool.cpp
    src/server/resource.cpp
    src/server/prompt.cpp
)

set(MCPP_CLIENT_SOURCES
    src/client/client.cpp
)

set(MCPP_ENTERPRISE_SOURCES
    src/enterprise/middleware.cpp
    src/enterprise/ratelimit.cpp
    src/enterprise/circuit.cpp
    src/enterprise/metrics.cpp
    src/enterprise/auth.cpp
)

set(MCPP_ALL_SOURCES
    ${MCPP_CORE_SOURCES}
    ${MCPP_PROTOCOL_SOURCES}
    ${MCPP_TRANSPORT_SOURCES}
    ${MCPP_SERVER_SOURCES}
    ${MCPP_CLIENT_SOURCES}
    ${MCPP_ENTERPRISE_SOURCES}
)

# Static library
add_library(mcpp_static STATIC ${MCPP_ALL_SOURCES})
target_compile_definitions(mcpp_static PUBLIC MCPP_STATIC)
set_target_properties(mcpp_static PROPERTIES OUTPUT_NAME mcpp)

# Shared library
add_library(mcpp_shared SHARED ${MCPP_ALL_SOURCES})
set_target_properties(mcpp_shared PROPERTIES OUTPUT_NAME mcpp)
target_compile_definitions(mcpp_shared PUBLIC MCPP_EXPORTS)

# Main library
add_library(mcpp ALIAS mcpp_static)

# Example executables
if(MCPP_BUILD_EXAMPLES)
    add_executable(example_stdio_server examples/stdio_server.cpp)
    target_link_libraries(example_stdio_server mcpp)

    add_executable(example_client examples/client_example.cpp)
    target_link_libraries(example_client mcpp)
endif()

# Tests
if(MCPP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install
install(DIRECTORY include/mcpp DESTINATION include)
install(TARGETS mcpp_static mcpp_shared
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

message(STATUS "mcpp v${PROJECT_VERSION} configured successfully")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
