cmake_minimum_required(VERSION 3.10)
project(deps NONE)

# C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(DEPS_PREFIX ${CMAKE_SOURCE_DIR}/../_deps)
set(DEPS_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/../_deps/install)
set(DEPS_DOWNLOAD_PREFIX ${CMAKE_SOURCE_DIR}/../_deps/src)

include(FetchContent)

# 1. nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.10.5
    GIT_SHALLOW TRUE
    SOURCE_DIR ${DEPS_DOWNLOAD_PREFIX}/nlohmann_json-src
    BINARY_DIR ${DEPS_PREFIX}/nlohmann_json-build
    INSTALL_DIR ${DEPS_INSTALL_PREFIX}/nlohmann_json
)
set(JSON_BuildTests OFF)
set(JSON_Install ON)
set(CMAKE_DISABLE_FINDING_PACKAGE_ZLIB ON)

# 2. cpp-httplib (with OpenSSL support)
FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
    GIT_SHALLOW TRUE
    SOURCE_DIR ${DEPS_DOWNLOAD_PREFIX}/cpp-httplib-src
    BINARY_DIR ${DEPS_PREFIX}/cpp-httplib-build
)
set(HTTPLIB_REQUIRE_OPENSSL ON)
set(HTTPLIB_REQUIRE_ZLIB OFF)
set(BUILD_SHARED_LIBS OFF)

# 3. GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
    SOURCE_DIR ${DEPS_DOWNLOAD_PREFIX}/googletest-src
    BINARY_DIR ${DEPS_PREFIX}/googletest-build
)
set(gtest_force_shared_crt ON)
set(BUILD_GMOCK ON)
set(BUILD_GTEST ON)
set(INSTALL_GTEST ON)

# Fetch and build dependencies in order
message(STATUS "Fetching nlohmann/json...")
FetchContent_MakeAvailable(nlohmann_json)

message(STATUS "Fetching cpp-httplib...")
FetchContent_MakeAvailable(cpp-httplib)

message(STATUS "Fetching GoogleTest...")
FetchContent_MakeAvailable(googletest)

# Install nlohmann/json to _deps/install
message(STATUS "Installing nlohmann/json...")
execute_process(
    COMMAND ${CMAKE_COMMAND} --install ${DEPS_PREFIX}/nlohmann_json-build
    WORKING_DIRECTORY ${DEPS_PREFIX}/nlohmann_json-build
)

message(STATUS "Dependencies installed successfully!")
message(STATUS "Install prefix: ${DEPS_INSTALL_PREFIX}")