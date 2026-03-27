/**
 * @file response.hpp
 * @brief Response building utilities
 */
#pragma once

#include "mcpp/core/types.hpp"
#include "mcpp/core/error.hpp"

namespace mcpp {

/**
 * @brief Builder for JSON-RPC responses
 */
class ResponseBuilder {
public:
    /**
     * @brief Create a success response
     * @param id Request ID
     * @param result Result value
     * @return Success response
     */
    static JsonRpcResponse success(const JsonValue& id, const JsonValue& result) {
        JsonRpcResponse resp;
        resp.id = id;
        resp.result = result;
        resp.is_error = false;
        return resp;
    }

    /**
     * @brief Create an error response with ErrorCode
     * @param id Request ID
     * @param code Error code
     * @param message Error message
     * @return Error response
     */
    static JsonRpcResponse error(const JsonValue& id, ErrorCode code, const std::string& message) {
        return error(id, static_cast<int>(code), message);
    }

    /**
     * @brief Create an error response with ErrorCode and data
     * @param id Request ID
     * @param code Error code
     * @param message Error message
     * @param data Additional error data
     * @return Error response
     */
    static JsonRpcResponse error(const JsonValue& id, ErrorCode code, const std::string& message, const JsonValue& data) {
        return error(id, static_cast<int>(code), message, data);
    }

    /**
     * @brief Create an error response with int code
     * @param id Request ID
     * @param code Error code
     * @param message Error message
     * @return Error response
     */
    static JsonRpcResponse error(const JsonValue& id, int code, const std::string& message) {
        return error(id, code, message, JsonValue::object());
    }

    /**
     * @brief Create an error response with int code and data
     * @param id Request ID
     * @param code Error code
     * @param message Error message
     * @param data Additional error data
     * @return Error response
     */
    static JsonRpcResponse error(const JsonValue& id, int code, const std::string& message, const JsonValue& data) {
        JsonRpcResponse resp;
        resp.id = id;
        resp.is_error = true;
        resp.error = {
            {"code", code},
            {"message", message}
        };
        if (!data.is_null()) {
            resp.error["data"] = data;
        }
        return resp;
    }
};

/**
 * @brief Convenience functions for creating responses
 */
namespace response {
    /**
     * @brief Create a success response with optional result
     * @param id Request ID
     * @param result Result value (default: empty object)
     * @return Success response
     */
    inline JsonRpcResponse ok(const JsonValue& id, const JsonValue& result = JsonValue::object()) {
        return ResponseBuilder::success(id, result);
    }

    /**
     * @brief Create a parse error response
     * @param id Request ID
     * @return Parse error response (-32700)
     */
    inline JsonRpcResponse parse_error(const JsonValue& id) {
        return ResponseBuilder::error(id, -32700, "Parse error");
    }

    /**
     * @brief Create an invalid request error response
     * @param id Request ID
     * @return Invalid request error response (-32600)
     */
    inline JsonRpcResponse invalid_request(const JsonValue& id) {
        return ResponseBuilder::error(id, -32600, "Invalid Request");
    }

    /**
     * @brief Create a method not found error response
     * @param id Request ID
     * @return Method not found error response (-32601)
     */
    inline JsonRpcResponse method_not_found(const JsonValue& id) {
        return ResponseBuilder::error(id, -32601, "Method not found");
    }

    /**
     * @brief Create an invalid params error response
     * @param id Request ID
     * @param message Custom error message
     * @return Invalid params error response (-32602)
     */
    inline JsonRpcResponse invalid_params(const JsonValue& id, const std::string& message = "Invalid params") {
        return ResponseBuilder::error(id, -32602, message);
    }

    /**
     * @brief Create an internal error response
     * @param id Request ID
     * @return Internal error response (-32603)
     */
    inline JsonRpcResponse internal_error(const JsonValue& id) {
        return ResponseBuilder::error(id, -32603, "Internal error");
    }
} // namespace response

} // namespace mcpp
