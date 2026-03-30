#!/bin/bash

# Dependency installation script for mcpp
# Replaces _deps/CMakeLists.txt functionality

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DEPS_PREFIX="${PROJECT_ROOT}/_deps"
DEPS_SRC_PREFIX="${DEPS_PREFIX}/src"
DEPS_BUILD_PREFIX="${DEPS_PREFIX}"
DEPS_INSTALL_PREFIX="${DEPS_PREFIX}/install"

# Dependency versions
NLOHMANN_JSON_VERSION="v3.10.5"
CPP_HTTPLIB_VERSION="v0.15.3"
GOOGLETEST_VERSION="v1.14.0"
LIBWEBSOCKETS_VERSION="v4.3.4"

# Create directories
mkdir -p "${DEPS_SRC_PREFIX}"
mkdir -p "${DEPS_BUILD_PREFIX}"
mkdir -p "${DEPS_INSTALL_PREFIX}"

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

download_dependency() {
    local name=$1
    local repo_url=$2
    local version=$3
    local dest_dir=$4

    log_info "Downloading ${name} ${version}..."

    if [ -d "${dest_dir}" ]; then
        log_warn "${name} already exists at ${dest_dir}, skipping download"
        return 0
    fi

    git clone --depth 1 --branch "${version}" "${repo_url}" "${dest_dir}"

    if [ $? -eq 0 ]; then
        log_info "${name} downloaded successfully"
    else
        log_error "Failed to download ${name}"
        return 1
    fi
}

# 1. nlohmann/json (header-only)
install_nlohmann_json() {
    local src_dir="${DEPS_SRC_PREFIX}/nlohmann_json-src"

    download_dependency "nlohmann/json" \
        "https://github.com/nlohmann/json.git" \
        "${NLOHMANN_JSON_VERSION}" \
        "${src_dir}"

    log_info "Installing nlohmann/json..."

    local build_dir="${DEPS_BUILD_PREFIX}/nlohmann_json-build"
    local install_dir="${DEPS_INSTALL_PREFIX}/nlohmann_json"

    mkdir -p "${build_dir}"
    cd "${build_dir}"

    cmake "${src_dir}" \
        -DCMAKE_INSTALL_PREFIX="${install_dir}" \
        -DJSON_BuildTests=OFF \
        -DJSON_Install=ON \
        -DCMAKE_BUILD_TYPE=Release

    make -j$(nproc)
    make install

    cd - > /dev/null
    log_info "nlohmann/json installed successfully"
}

# 2. cpp-httplib (header-only)
install_cpp_httplib() {
    local src_dir="${DEPS_SRC_PREFIX}/cpp-httplib-src"

    download_dependency "cpp-httplib" \
        "https://github.com/yhirose/cpp-httplib.git" \
        "${CPP_HTTPLIB_VERSION}" \
        "${src_dir}"

    log_info "cpp-httplib is header-only, no build needed"
}

# 3. GoogleTest
install_googletest() {
    local src_dir="${DEPS_SRC_PREFIX}/googletest-src"

    download_dependency "GoogleTest" \
        "https://github.com/google/googletest.git" \
        "${GOOGLETEST_VERSION}" \
        "${src_dir}"

    log_info "Building GoogleTest..."

    local build_dir="${DEPS_BUILD_PREFIX}/googletest-build"

    mkdir -p "${build_dir}"
    cd "${build_dir}"

    cmake "${src_dir}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -Dgtest_force_shared_crt=ON \
        -DBUILD_GMOCK=ON \
        -DBUILD_GTEST=ON \
        -DCMAKE_INSTALL_PREFIX="${DEPS_PREFIX}"

    make -j$(nproc)

    # Copy libraries to _deps/lib
    mkdir -p "${DEPS_PREFIX}/lib"
    cp "${build_dir}/lib/libgtest.a" "${DEPS_PREFIX}/lib/" 2>/dev/null || cp "${build_dir}/googlemock/gtest/libgtest.a" "${DEPS_PREFIX}/lib/"
    cp "${build_dir}/lib/libgtest_main.a" "${DEPS_PREFIX}/lib/" 2>/dev/null || cp "${build_dir}/googlemock/gtest/libgtest_main.a" "${DEPS_PREFIX}/lib/"
    cp "${build_dir}/lib/libgmock.a" "${DEPS_PREFIX}/lib/" 2>/dev/null || cp "${build_dir}/googlemock/libgmock.a" "${DEPS_PREFIX}/lib/"
    cp "${build_dir}/lib/libgmock_main.a" "${DEPS_PREFIX}/lib/" 2>/dev/null || cp "${build_dir}/googlemock/libgmock_main.a" "${DEPS_PREFIX}/lib/"

    cd - > /dev/null
    log_info "GoogleTest built successfully"
}

# 4. libwebsockets
install_libwebsockets() {
    local src_dir="${DEPS_SRC_PREFIX}/libwebsockets-src"

    download_dependency "libwebsockets" \
        "https://github.com/warmcat/libwebsockets.git" \
        "${LIBWEBSOCKETS_VERSION}" \
        "${src_dir}"

    log_info "Building libwebsockets..."

    local build_dir="${DEPS_BUILD_PREFIX}/libwebsockets-build"

    mkdir -p "${build_dir}"
    cd "${build_dir}"

    cmake "${src_dir}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DLWS_WITH_CLIENT=ON \
        -DLWS_WITH_SERVER=ON \
        -DLWS_WITH_WSS=ON \
        -DLWS_WITH_SSL=ON \
        -DLWS_WITH_HTTP2=OFF \
        -DLWS_WERROR=OFF \
        -DLWS_WITHOUT_TESTAPPS=ON \
        -DLWS_WITHOUT_TEST_SERVER=ON \
        -DLWS_WITHOUT_TEST_SERVER_EXTPOLL=ON \
        -DLWS_WITHOUT_TEST_PING=ON \
        -DLWS_WITHOUT_TEST_CLIENT=ON \
        -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON

    make -j$(nproc)

    cd - > /dev/null
    log_info "libwebsockets built successfully"
}

# Clean all dependencies
clean_deps() {
    log_info "Cleaning all dependencies..."
    rm -rf "${DEPS_PREFIX}"
    log_info "Dependencies cleaned"
}

# Show help
show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

Install dependencies for mcpp project.

OPTIONS:
    -h, --help      Show this help message
    -c, --clean     Clean all dependencies and rebuild
    -j, --jobs N    Number of parallel jobs (default: $(nproc))

DEPENDENCIES:
    - nlohmann/json ${NLOHMANN_JSON_VERSION}
    - cpp-httplib ${CPP_HTTPLIB_VERSION}
    - GoogleTest ${GOOGLETEST_VERSION}
    - libwebsockets ${LIBWEBSOCKETS_VERSION}

EXAMPLES:
    $0              # Install all dependencies
    $0 -c           # Clean and reinstall all dependencies
    $0 -j 4         # Use 4 parallel jobs

EOF
}

# Main
main() {
    local clean=false
    local jobs=$(nproc)

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -c|--clean)
                clean=true
                shift
                ;;
            -j|--jobs)
                jobs="$2"
                shift 2
                ;;
            *)
                log_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # Override make jobs
    MAKE_JOBS="-j${jobs}"

    log_info "Installing dependencies to ${DEPS_PREFIX}"
    log_info "Using ${jobs} parallel jobs"

    # Clean if requested
    if [ "$clean" = true ]; then
        clean_deps
        mkdir -p "${DEPS_SRC_PREFIX}"
        mkdir -p "${DEPS_BUILD_PREFIX}"
        mkdir -p "${DEPS_INSTALL_PREFIX}"
    fi

    # Check for required tools
    command -v git >/dev/null 2>&1 || { log_error "git is required but not installed"; exit 1; }
    command -v cmake >/dev/null 2>&1 || { log_error "cmake is required but not installed"; exit 1; }
    command -v make >/dev/null 2>&1 || { log_error "make is required but not installed"; exit 1; }

    # Install dependencies in order
    install_nlohmann_json
    install_cpp_httplib
    install_googletest
    install_libwebsockets

    log_info "All dependencies installed successfully!"
    log_info "Install prefix: ${DEPS_INSTALL_PREFIX}"
}

main "$@"
