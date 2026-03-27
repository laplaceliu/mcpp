/**
 * @file types.hpp
 * @brief Core MCP protocol types and structures
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include "json.hpp"

namespace mcpp {

/**
 * @brief JSON-RPC request structure
 */
struct JsonRpcRequest {
    std::string jsonrpc = "2.0";   ///< JSON-RPC version, must be "2.0"
    std::string method;             ///< Method name to invoke
    JsonValue id;                   ///< Request identifier (null for notifications)
    JsonValue params;               ///< Method parameters (optional)
};

/**
 * @brief JSON-RPC response structure
 */
struct JsonRpcResponse {
    std::string jsonrpc = "2.0";   ///< JSON-RPC version, must be "2.0"
    JsonValue id;                   ///< Response identifier matching request
    JsonValue result;               ///< Result value (for success responses)
    JsonValue error;                ///< Error object (for error responses)
    bool is_error = false;          ///< True if this is an error response
};

/**
 * @brief JSON-RPC error object
 */
struct JsonRpcError {
    int code;                       ///< Error code
    std::string message;            ///< Error message
    JsonValue data;                 ///< Additional error data (optional)
};

/**
 * @brief Server capabilities supported by the MCP server
 */
struct ServerCapabilities {
    bool supports_tools = false;      ///< Whether tools are supported
    bool supports_resources = false;  ///< Whether resources are supported
    bool supports_prompts = false;    ///< Whether prompts are supported
    bool supports_sampling = false;   ///< Whether sampling is supported
    bool supports_elicitation = false;///< Whether elicitation is supported
    bool supports_logging = false;    ///< Whether logging is supported
};

/**
 * @brief Client capabilities
 */
struct ClientCapabilities {
    bool supports_tools = false;
    bool supports_resources = false;
    bool supports_prompts = false;
    bool supports_sampling = false;
    bool supports_elicitation = false;
    bool supports_logging = false;
};

// ============ Initialization ============

/**
 * @brief Parameters for initialize request
 */
struct InitializeParams {
    std::string protocol_version;     ///< Client's protocol version
    ClientCapabilities client_info;   ///< Client information
    ClientCapabilities capabilities;  ///< Client capabilities
};

/**
 * @brief Result of initialize request
 */
struct InitializeResult {
    std::string protocol_version;     ///< Server's protocol version
    ServerCapabilities server_info;    ///< Server information
    ClientCapabilities capabilities;   ///< Server capabilities
};

// ============ Tools ============

/**
 * @brief Tool definition
 */
struct Tool {
    std::string name;                ///< Tool name (unique identifier)
    std::string description;         ///< Human-readable description
    JsonValue input_schema;           ///< JSON Schema for tool input
};

/**
 * @brief Result of tools/list request
 */
struct ListToolsResult {
    std::vector<Tool> tools;          ///< List of available tools
};

/**
 * @brief Result of tools/call request
 */
struct CallToolResult {
    bool is_error;                     ///< Whether the tool call resulted in error
    std::vector<JsonValue> content;    ///< Tool output content
    std::string error;                 ///< Error message if is_error is true
};

// ============ Resources ============

/**
 * @brief Resource definition
 */
struct Resource {
    std::string uri;                   ///< Resource URI
    std::string name;                  ///< Human-readable name
    std::string description;           ///< Resource description
    std::string mime_type;             ///< MIME type of the resource
};

/**
 * @brief Resource template for parameterized resources
 */
struct ResourceTemplate {
    std::string uri_template;          ///< URI template pattern
    std::string name;                  ///< Template name
    std::string description;           ///< Template description
    std::string mime_type;             ///< MIME type
};

/**
 * @brief Result of resources/list request
 */
struct ListResourcesResult {
    std::vector<Resource> resources;           ///< Static resources
    std::vector<ResourceTemplate> resource_templates; ///< Resource templates
};

// ============ Prompts ============

/**
 * @brief Prompt definition
 */
struct Prompt {
    std::string name;                   ///< Prompt name (unique identifier)
    std::string description;            ///< Prompt description
    JsonValue arguments;                ///< Argument definitions (optional)
};

/**
 * @brief Message in a prompt
 */
struct PromptMessage {
    std::string role;                   ///< "user" or "assistant"
    JsonValue content;                  ///< Message content
};

/**
 * @brief Result of prompts/get request
 */
struct GetPromptResult {
    std::string description;            ///< Prompt description
    std::vector<PromptMessage> messages;///< Prompt messages
};

// ============ Notifications ============

/**
 * @brief Parameters for cancel request
 */
struct CancelParams {
    JsonValue id;                       ///< ID of request to cancel
};

/**
 * @brief Parameters for logging message notification
 */
struct LoggingMessageParams {
    std::string level;                  ///< Log level (debug, info, warning, error, etc.)
    JsonValue data;                     ///< Log data
};

// ============ Helper Functions ============

/**
 * @brief Create a success response
 * @param id Request ID
 * @return JsonRpcResponse configured as success
 */
inline JsonRpcResponse create_response(const JsonValue& id) {
    JsonRpcResponse resp;
    resp.id = id;
    return resp;
}

/**
 * @brief Create an error response
 * @param id Request ID
 * @param code Error code
 * @param message Error message
 * @return Configured error response
 */
inline JsonRpcResponse create_error_response(const JsonValue& id, int code, const std::string& message) {
    JsonRpcResponse resp;
    resp.id = id;
    resp.is_error = true;
    resp.error = JsonValue::object({{"code", code}, {"message", message}});
    return resp;
}

} // namespace mcpp
