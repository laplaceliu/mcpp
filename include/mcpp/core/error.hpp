/**
 * @file error.hpp
 * @brief Error handling types and utilities for MCPP
 */
#pragma once

#include <string>
#include <stdexcept>

namespace mcpp {

/**
 * @brief Error codes used in MCPP
 */
enum class ErrorCode {
    ParseError = -32700,           ///< Invalid JSON was received
    InvalidRequest = -32600,       ///< The JSON sent is not a valid Request object
    MethodNotFound = -32601,       ///< The method does not exist or is not available
    InvalidParams = -32602,        ///< Invalid method parameter(s)
    InternalError = -32603,         ///< Internal MCPP error
    // MCP specific errors
    ConnectionClosed = -32000,     ///< Connection was closed unexpectedly
    Timeout = -32001,              ///< Operation timed out
    TransportError = -32002,        ///< Transport layer error
};

/**
 * @brief Exception class for MCPP errors
 */
class McppError : public std::runtime_error {
public:
    /**
     * @brief Construct an McppError
     * @param code Error code
     * @param message Error message
     */
    McppError(ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    /**
     * @brief Get the error code
     * @return ErrorCode
     */
    ErrorCode code() const { return code_; }

private:
    ErrorCode code_;
};

/**
 * @brief Result type for operations that can fail
 * @tparam T Type of the success value
 */
template<typename T>
class Result {
public:
    /**
     * @brief Construct a success result
     * @param value The success value
     */
    Result(const T& value) : value_(value), has_error_(false) {}

    /**
     * @brief Construct an error result
     * @param error The error message
     */
    Result(const std::string& error) : error_(error), has_error_(true) {}

    /**
     * @brief Check if the result is successful
     * @return true if success, false if error
     */
    bool ok() const { return !has_error_; }

    /**
     * @brief Get the success value
     * @return Reference to the value
     */
    const T& value() const { return value_; }

    /**
     * @brief Get the error message
     * @return Reference to the error string
     */
    const std::string& error() const { return error_; }

private:
    T value_;
    std::string error_;
    bool has_error_;
};

/**
 * @brief Macro to suppress unused variable warnings
 */
#define MCPP_UNUSED(x) (void)(x)

} // namespace mcpp
