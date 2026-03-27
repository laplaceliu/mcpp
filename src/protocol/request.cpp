#include "mcpp/protocol/request.hpp"
#include <utility>

namespace mcpp {

// 内部 HandlerWrapper 类
class HandlerWrapper : public IRequestHandler {
public:
    HandlerWrapper(RequestHandler handler) : handler_(std::move(handler)) {}

    JsonRpcResponse handle(const JsonRpcRequest& request) override {
        return handler_(request);
    }

private:
    RequestHandler handler_;
};

// ============ RequestRouter ============

void RequestRouter::register_handler(const std::string& method, RequestHandler handler) {
    handlers_[method] = std::shared_ptr<IRequestHandler>(new HandlerWrapper(std::move(handler)));
}

void RequestRouter::register_handler(const std::string& method, std::shared_ptr<IRequestHandler> handler) {
    handlers_[method] = handler;
}

Result<JsonRpcResponse> RequestRouter::route(const JsonRpcRequest& request) {
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

bool RequestRouter::has_handler(const std::string& method) const {
    return handlers_.find(method) != handlers_.end();
}

// ============ InitializeHandler ============

InitializeHandler::InitializeHandler() {
    capabilities_.supports_tools = true;
    capabilities_.supports_resources = true;
    capabilities_.supports_prompts = true;
}

JsonRpcResponse InitializeHandler::handle(const JsonRpcRequest& request) {
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

void InitializeHandler::set_server_info(const std::string& name, const std::string& version) {
    server_name_ = name;
    server_version_ = version;
}

// ============ ToolsListHandler ============

JsonRpcResponse ToolsListHandler::handle(const JsonRpcRequest& request) {
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

void ToolsListHandler::add_tool(const Tool& tool) {
    tools_.push_back(tool);
}

void ToolsListHandler::add_tool(Tool&& tool) {
    tools_.push_back(std::move(tool));
}

// ============ ToolsCallHandler ============

JsonRpcResponse ToolsCallHandler::handle(const JsonRpcRequest& request) {
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

// ============ RequestContext ============

void RequestContext::set_result(const JsonValue& result) {
    response_.is_error = false;
    response_.result = result;
}

void RequestContext::set_error(int code, const std::string& message, const JsonValue& data) {
    response_.is_error = true;
    response_.error = JsonValue::object({
        {"code", code},
        {"message", message}
    });
    if (!data.is_null()) {
        response_.error["data"] = data;
    }
}

} // namespace mcpp
