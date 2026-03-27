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

# nlohmann/json (header-only library - use source directly)
set(NLOHMANN_JSON_INCLUDE_DIR ${DEPS_SRC_PREFIX}/nlohmann_json-src/single_include)
if(NOT EXISTS ${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp)
    message(FATAL_ERROR "nlohmann/json not found at ${NLOHMANN_JSON_INCLUDE_DIR}")
endif()

# cpp-httplib (header-only library)
set(HTTPLIB_INCLUDE_DIR ${DEPS_SRC_PREFIX}/cpp-httplib-src)
if(NOT EXISTS ${HTTPLIB_INCLUDE_DIR}/httplib.h)
    message(FATAL_ERROR "cpp-httplib not found at ${HTTPLIB_INCLUDE_DIR}")
endif()

# OpenSSL (required for WebSocket)
find_package(OpenSSL REQUIRED)

# libwebsockets (WebSocket library)
set(LIBWEBSOCKETS_INCLUDE_DIR ${DEPS_SRC_PREFIX}/libwebsockets-src/include)
set(LIBWEBSOCKETS_BUILD_DIR ${DEPS_PREFIX}/libwebsockets-build/include)
if(NOT EXISTS ${LIBWEBSOCKETS_INCLUDE_DIR}/libwebsockets.h)
    message(FATAL_ERROR "libwebsockets not found at ${LIBWEBSOCKETS_INCLUDE_DIR}")
endif()
set(LIBWEBSOCKETS_LIBRARY ${DEPS_PREFIX}/libwebsockets-build/lib/libwebsockets.so)
if(NOT EXISTS ${LIBWEBSOCKETS_LIBRARY})
    message(FATAL_ERROR "libwebsockets library not found at ${LIBWEBSOCKETS_LIBRARY}")
endif()

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
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${HTTPLIB_INCLUDE_DIR}
    ${LIBWEBSOCKETS_INCLUDE_DIR}
    ${LIBWEBSOCKETS_BUILD_DIR}
)

# Source files
# Note: All implementations are now header-only.
# No .cpp files need to be compiled for the mcpp library itself.

# Header-only library - use INTERFACE library
add_library(mcpp INTERFACE)
target_include_directories(mcpp INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mcpp INTERFACE OpenSSL::SSL OpenSSL::Crypto ${LIBWEBSOCKETS_LIBRARY})

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

# Install - header-only library, only install headers
install(DIRECTORY include/mcpp DESTINATION include)
install(TARGETS mcpp
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

message(STATUS "mcpp v${PROJECT_VERSION} configured successfully")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
