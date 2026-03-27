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
 * @brief Internal HandlerWrapper class wrapping request handlers
 */
class HandlerWrapper : public IRequestHandler {
public:
    HandlerWrapper(RequestHandler handler) : handler_(std::move(handler)) {}

    JsonRpcResponse handle(const JsonRpcRequest& request) override {
        return handler_(request);
    }

private:
    RequestHandler handler_;
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
    void register_handler(const std::string& method, RequestHandler handler) {
        handlers_[method] = std::shared_ptr<IRequestHandler>(new HandlerWrapper(std::move(handler)));
    }

    /**
     * @brief Register a handler object for a method
     * @param method Method name
     * @param handler Handler instance
     */
    void register_handler(const std::string& method, std::shared_ptr<IRequestHandler> handler) {
        handlers_[method] = handler;
    }

    /**
     * @brief Route a request to the appropriate handler
     * @param request Request to route
     * @return Result containing response or error
     */
    Result<JsonRpcResponse> route(const JsonRpcRequest& request) {
        auto it = handlers_.find(request.method);
        if (it == handlers_.end()) {
            return Result<JsonRpcResponse>("Method not found: " + request.method);
        }

        try {
            auto response = it->second->handle(request);
            return Result<JsonRpcResponse>(response);
        } catch (const std::exception& e) {
            return Result<JsonRpcResponse>(std::string("Handler error: ") + e.what());
        }
    }

    /**
     * @brief Check if a handler exists for a method
     * @param method Method name
     * @return true if handler exists
     */
    bool has_handler(const std::string& method) const {
        return handlers_.find(method) != handlers_.end();
    }

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
    void set_result(const JsonValue& result) {
        response_.is_error = false;
        response_.result = result;
    }

    /**
     * @brief Set an error response
     * @param code Error code
     * @param message Error message
     * @param data Additional error data
     */
    void set_error(int code, const std::string& message, const JsonValue& data = JsonValue::object()) {
        response_.is_error = true;
        response_.error = JsonValue::object({
            {"code", code},
            {"message", message}
        });
        if (!data.is_null()) {
            response_.error["data"] = data;
        }
    }

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
    InitializeHandler() {
        capabilities_.supports_tools = true;
        capabilities_.supports_resources = true;
        capabilities_.supports_prompts = true;
    }

    JsonRpcResponse handle(const JsonRpcRequest& request) override {
        JsonRpcResponse resp;
        resp.id = request.id;

        resp.result = JsonValue::object();
        resp.result["protocolVersion"] = "2025-06-18";
        resp.result["serverInfo"] = JsonValue::object({{"name", server_name_.empty() ? "mcpp-server" : server_name_}, {"version", server_version_.empty() ? "1.0.0" : server_version_}});
        resp.result["capabilities"] = JsonValue::object({
            {"tools", capabilities_.supports_tools},
            {"resources", capabilities_.supports_resources},
            {"prompts", capabilities_.supports_prompts}
        });
        resp.is_error = false;

        return resp;
    }

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
    void set_server_info(const std::string& name, const std::string& version) {
        server_name_ = name;
        server_version_ = version;
    }

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
    JsonRpcResponse handle(const JsonRpcRequest& request) override {
        JsonRpcResponse resp;
        resp.id = request.id;

        JsonValue tools_array = JsonValue::array();
        for (const auto& tool : tools_) {
            tools_array.push_back(JsonValue::object({
                {"name", tool.name},
                {"description", tool.description},
                {"inputSchema", tool.input_schema}
            }));
        }

        resp.result = JsonValue::object({{"tools", tools_array}});
        resp.is_error = false;

        return resp;
    }

    /**
     * @brief Add a tool to the list
     * @param tool Tool to add
     */
    void add_tool(const Tool& tool) {
        tools_.push_back(tool);
    }

    /**
     * @brief Add a tool (move version)
     * @param tool Tool to add
     */
    void add_tool(Tool&& tool) {
        tools_.push_back(std::move(tool));
    }

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

    JsonRpcResponse handle(const JsonRpcRequest& request) override {
        JsonRpcResponse resp;
        resp.id = request.id;

        if (!call_func_) {
            resp.is_error = true;
            resp.error = JsonValue::object({{"code", -32603}, {"message", "No handler registered"}});
            return resp;
        }

        std::string name;
        JsonValue args;

        if (request.params.is_object()) {
            name = request.params.value("name", std::string());
            args = request.params.value("arguments", JsonValue::object());
        }

        if (name.empty()) {
            resp.is_error = true;
            resp.error = JsonValue::object({{"code", -32602}, {"message", "Missing tool name"}});
            return resp;
        }

        try {
            CallToolResult result = call_func_(name, args);
            if (result.is_error) {
                resp.is_error = true;
                resp.error = JsonValue::object({{"code", -32603}, {"message", result.error}});
            } else {
                resp.result = JsonValue::object({{"content", result.content}});
                resp.is_error = false;
            }
        } catch (const std::exception& e) {
            resp.is_error = true;
            resp.error = JsonValue::object({{"code", -32603}, {"message", e.what()}});
        }

        return resp;
    }

    /**
     * @brief Set the tool execution function
     * @param func Function to call for tool execution
     */
    void set_call_function(CallToolFunc func) { call_func_ = std::move(func); }

private:
    CallToolFunc call_func_;
};

} // namespace mcpp
