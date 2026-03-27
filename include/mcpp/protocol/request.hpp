/**
 * @file request.hpp
 * @brief Request handling and routing
 */
#pragma once

#include <functional>
#include <memory>
#include <map>
#include "mcpp/core/types.hpp"
#include "mcpp/core/error.hpp"

namespace mcpp {

/**
 * @brief Request handler function type
 */
using RequestHandler = std::function<JsonRpcResponse(const JsonRpcRequest&)>;

/**
 * @brief Interface for request handlers
 */
class IRequestHandler {
public:
    virtual ~IRequestHandler() = default;

    /**
     * @brief Handle a JSON-RPC request
     * @param request The request to handle
     * @return Response to send back
     */
    virtual JsonRpcResponse handle(const JsonRpcRequest& request) = 0;
};

/**
 * @brief Routes requests to appropriate handlers
 */
class RequestRouter {
public:
    /**
     * @brief Register a handler function for a method
     * @param method Method name
     * @param handler Handler function
     */
    void register_handler(const std::string& method, RequestHandler handler);

    /**
     * @brief Register a handler object for a method
     * @param method Method name
     * @param handler Handler instance
     */
    void register_handler(const std::string& method, std::shared_ptr<IRequestHandler> handler);

    /**
     * @brief Route a request to the appropriate handler
     * @param request Request to route
     * @return Result containing response or error
     */
    Result<JsonRpcResponse> route(const JsonRpcRequest& request);

    /**
     * @brief Check if a handler exists for a method
     * @param method Method name
     * @return true if handler exists
     */
    bool has_handler(const std::string& method) const;

private:
    std::map<std::string, std::shared_ptr<IRequestHandler>> handlers_;
};

/**
 * @brief Context for request handling
 */
class RequestContext {
public:
    /**
     * @brief Get the current request
     * @return Reference to the request
     */
    const JsonRpcRequest& request() const { return request_; }

    /**
     * @brief Get the response being built
     * @return Reference to the response
     */
    JsonRpcResponse& response() { return response_; }

    /**
     * @brief Set the current request
     * @param req Request to set
     */
    void set_request(const JsonRpcRequest& req) { request_ = req; }

    /**
     * @brief Set the result of the request
     * @param result Result value
     */
    void set_result(const JsonValue& result);

    /**
     * @brief Set an error response
     * @param code Error code
     * @param message Error message
     * @param data Additional error data
     */
    void set_error(int code, const std::string& message, const JsonValue& data = JsonValue::object());

private:
    JsonRpcRequest request_;
    JsonRpcResponse response_;
};

// ============ MCP Request Handlers ============

/**
 * @brief Handler for initialize request
 */
class InitializeHandler : public IRequestHandler {
public:
    InitializeHandler();

    JsonRpcResponse handle(const JsonRpcRequest& request) override;

    /**
     * @brief Set server capabilities
     * @param caps Capabilities to advertise
     */
    void set_capabilities(const ServerCapabilities& caps) { capabilities_ = caps; }

    /**
     * @brief Set server information
     * @param name Server name
     * @param version Server version
     */
    void set_server_info(const std::string& name, const std::string& version);

private:
    ServerCapabilities capabilities_;
    std::string server_name_;
    std::string server_version_;
};

/**
 * @brief Handler for tools/list request
 */
class ToolsListHandler : public IRequestHandler {
public:
    JsonRpcResponse handle(const JsonRpcRequest& request) override;

    /**
     * @brief Add a tool to the list
     * @param tool Tool to add
     */
    void add_tool(const Tool& tool);

    /**
     * @brief Add a tool (move version)
     * @param tool Tool to add
     */
    void add_tool(Tool&& tool);

    /**
     * @brief Get all registered tools
     * @return Vector of tools
     */
    const std::vector<Tool>& tools() const { return tools_; }

private:
    std::vector<Tool> tools_;
};

/**
 * @brief Handler for tools/call request
 */
class ToolsCallHandler : public IRequestHandler {
public:
    /**
     * @brief Function type for tool execution
     */
    using CallToolFunc = std::function<CallToolResult(const std::string&, const JsonValue&)>;

    JsonRpcResponse handle(const JsonRpcRequest& request) override;

    /**
     * @brief Set the tool execution function
     * @param func Function to call for tool execution
     */
    void set_call_function(CallToolFunc func) { call_func_ = std::move(func); }

private:
    CallToolFunc call_func_;
};

} // namespace mcpp
