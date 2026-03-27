/**
 * @file server.cpp
 * @brief MCP Server implementation
 */
#include "mcpp/server/server.hpp"
#include "mcpp/server/resource.hpp"
#include "mcpp/server/prompt.hpp"
#include "mcpp/transport/transport.hpp"
#include "mcpp/protocol/message.hpp"

namespace mcpp {

Server::Server(const ServerOptions& options)
    : options_(options)
    , running_(false)
{
    setup_handlers();
}

Server::~Server() {
    stop();
}

void Server::setup_handlers() {
    // Set up server info
    initialize_handler_.set_server_info(options_.name, options_.version);
    initialize_handler_.set_capabilities(options_.capabilities);

    // Register request handlers
    router_.register_handler("initialize", std::make_shared<InitializeHandler>(initialize_handler_));
    router_.register_handler("tools/list", std::make_shared<ToolsListHandler>(tools_list_handler_));
    router_.register_handler("tools/call", std::make_shared<ToolsCallHandler>(tools_call_handler_));
    router_.register_handler("resources/list", std::make_shared<ResourcesListHandler>(resources_list_handler_));
    router_.register_handler("resources/read", std::make_shared<ResourceReadHandler>(resource_read_handler_));
    router_.register_handler("resources/subscribe", std::make_shared<ResourceSubscribeHandler>());
    router_.register_handler("prompts/list", std::make_shared<PromptsListHandler>(prompts_list_handler_));
    router_.register_handler("prompts/get", std::make_shared<PromptsGetHandler>(prompts_get_handler_));
}

bool Server::start() {
    if (running_) return true;

    transport_ = TransportFactory::create(options_.transport_type);
    if (!transport_) {
        return false;
    }

    transport_->on_message([this](const std::string& msg) {
        handle_message(msg);
    });

    transport_->on_error([this](const std::string& err) {
        // Handle error
        MCPP_UNUSED(err);
    });

    if (!transport_->start()) {
        return false;
    }

    running_ = true;
    return true;
}

void Server::stop() {
    if (!running_) return;
    running_ = false;
    if (transport_) {
        transport_->stop();
    }
}

void Server::wait() {
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void Server::handle_message(const std::string& message) {
    auto parse_result = MessageParser::parse(message);
    if (!parse_result.ok()) {
        // Send parse error
        return;
    }

    auto& msg = parse_result.value();
    if (msg.type() == MessageType::Request || msg.type() == MessageType::Notification) {
        JsonRpcResponse resp;
        handle_request(msg.request, resp);

        // Send response for requests (not notifications)
        if (msg.type() == MessageType::Request && !resp.id.is_null()) {
            auto resp_str = MessageSerializer::serialize(resp);
            transport_->send(resp_str);
        }
    }
}

void Server::handle_request(const JsonRpcRequest& request, JsonRpcResponse& response) {
    auto result = router_.route(request);
    if (result.ok()) {
        response = result.value();
    } else {
        response.id = request.id;
        response.is_error = true;
        response.error = JsonValue::object({
            {"code", -32601},
            {"message", result.error()}
        });
    }
}

void Server::send_notification(const std::string& method, const JsonValue& params) {
    JsonRpcRequest notif;
    notif.jsonrpc = "2.0";
    notif.method = method;
    notif.params = params;

    auto msg = MessageSerializer::serialize(notif);
    transport_->send(msg);
}

// ============ Tool Registration ============

void Server::register_tool(const std::string& name,
                           const std::string& description,
                           const JsonValue& input_schema,
                           std::function<CallToolResult(const std::string&, const JsonValue&)> handler) {
    Tool tool;
    tool.name = name;
    tool.description = description;
    tool.input_schema = input_schema;

    tools_list_handler_.add_tool(std::move(tool));

    // Set up call handler
    tools_call_handler_.set_call_function(std::move(handler));
}

std::vector<Tool> Server::list_tools() const {
    return tools_list_handler_.tools();
}

// ============ Resource Registration ============

void Server::register_resource(const std::string& uri,
                               const std::string& name,
                               const std::string& description,
                               const std::string& mime_type,
                               std::function<JsonValue()> handler) {
    resources_list_handler_.add_resource(uri, name, description, mime_type, std::move(handler));
}

void Server::register_resource_template(const std::string& uri_template,
                                        const std::string& name,
                                        const std::string& description,
                                        const std::string& mime_type) {
    resources_list_handler_.add_resource_template(uri_template, name, description, mime_type);
}

// ============ Prompt Registration ============

void Server::register_prompt(const std::string& name,
                             const std::string& description,
                             std::function<GetPromptResult(const JsonValue&)> handler) {
    prompts_list_handler_.add_prompt(name, description, JsonValue::object(), std::move(handler));
}

// ============ Notifications ============

void Server::notify_tools_changed() {
    send_notification("notifications/tools_changed", JsonValue::object());
}

void Server::notify_resources_changed() {
    send_notification("notifications/resources_changed", JsonValue::object());
}

void Server::notify_resource_updated(const std::string& uri) {
    send_notification("notifications/resources/updated", JsonValue::object({{"uri", uri}}));
}

void Server::log(const std::string& level, const JsonValue& data) {
    send_notification("logging/message", JsonValue::object({
        {"level", level},
        {"data", data}
    }));
}

} // namespace mcpp
