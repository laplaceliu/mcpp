/**
 * @file server.hpp
 * @brief MCP Server implementation
 */
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <functional>
#include "mcpp/core/types.hpp"
#include "mcpp/protocol/message.hpp"
#include "mcpp/protocol/request.hpp"
#include "mcpp/protocol/response.hpp"
#include "mcpp/transport/transport.hpp"
#include "mcpp/server/resource.hpp"
#include "mcpp/server/prompt.hpp"

namespace mcpp {

struct ServerOptions {
    std::string name = "mcpp-server";          ///< Server name
    std::string version = "1.0.0";             ///< Server version
    std::string protocol_version = "2025-06-18"; ///< MCP protocol version

    ServerCapabilities capabilities;           ///< Server capabilities

    TransportFactory::Type transport_type = TransportFactory::Type::Stdio; ///< Transport type
    int port = 8080;                           ///< Port for HTTP transport
};

/**
 * @brief Main MCP Server class
 * @details Implements the Model Context Protocol server-side functionality
 */
class Server : public std::enable_shared_from_this<Server> {
public:
    /**
     * @brief Construct a server
     * @param options Server configuration options
     */
    explicit Server(const ServerOptions& options = ServerOptions())
        : options_(options), running_(false) {
        setup_handlers();
    }

    /**
     * @brief Destructor - stops the server
     */
    ~Server() {
        stop();
    }

    // ============ Lifecycle ============

    /**
     * @brief Start the server
     * @return true if started successfully
     */
    bool start() {
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

    /**
     * @brief Stop the server
     */
    void stop() {
        if (!running_) return;
        running_ = false;
        if (transport_) {
            transport_->stop();
        }
    }

    /**
     * @brief Wait for server thread to complete
     */
    void wait() {
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    // ============ Tool Registration ============

    /**
     * @brief Register a tool with the server
     * @param name Tool name (unique identifier)
     * @param description Human-readable description
     * @param input_schema JSON Schema for tool arguments
     * @param handler Function to execute the tool (takes name and args, returns CallToolResult)
     */
    void register_tool(const std::string& name,
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

    /**
     * @brief Get list of registered tools
     * @return Vector of registered tools
     */
    std::vector<Tool> list_tools() const {
        return tools_list_handler_.tools();
    }

    // ============ Resource Registration ============

    /**
     * @brief Register a resource
     * @param uri Resource URI
     * @param name Resource name
     * @param description Resource description
     * @param mime_type MIME type of resource content
     * @param handler Function to read the resource
     */
    void register_resource(const std::string& uri,
                           const std::string& name,
                           const std::string& description,
                           const std::string& mime_type,
                           std::function<JsonValue()> handler) {
        resources_list_handler_.add_resource(uri, name, description, mime_type, std::move(handler));
    }

    /**
     * @brief Register a resource template
     * @param uri_template URI template pattern
     * @param name Template name
     * @param description Template description
     * @param mime_type MIME type
     */
    void register_resource_template(const std::string& uri_template,
                                    const std::string& name,
                                    const std::string& description,
                                    const std::string& mime_type) {
        resources_list_handler_.add_resource_template(uri_template, name, description, mime_type);
    }

    // ============ Prompt Registration ============

    /**
     * @brief Register a prompt template
     * @param name Prompt name
     * @param description Prompt description
     * @param handler Function to generate prompt
     */
    void register_prompt(const std::string& name,
                         const std::string& description,
                         std::function<GetPromptResult(const JsonValue&)> handler) {
        prompts_list_handler_.add_prompt(name, description, JsonValue::object(), std::move(handler));
    }

    // ============ Notifications ============

    /**
     * @brief Send tools changed notification
     */
    void notify_tools_changed() {
        send_notification("notifications/tools_changed", JsonValue::object());
    }

    /**
     * @brief Send resources changed notification
     */
    void notify_resources_changed() {
        send_notification("notifications/resources_changed", JsonValue::object());
    }

    /**
     * @brief Send resource updated notification
     * @param uri URI of updated resource
     */
    void notify_resource_updated(const std::string& uri) {
        send_notification("notifications/resources/updated", JsonValue::object({{"uri", uri}}));
    }

    /**
     * @brief Send a log message
     * @param level Log level
     * @param data Log data
     */
    void log(const std::string& level, const JsonValue& data) {
        send_notification("logging/message", JsonValue::object({
            {"level", level},
            {"data", data}
        }));
    }

private:
    void setup_handlers() {
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

    void handle_message(const std::string& message) {
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

    void handle_request(const JsonRpcRequest& request, JsonRpcResponse& response) {
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

    void send_notification(const std::string& method, const JsonValue& params) {
        JsonRpcRequest notif;
        notif.jsonrpc = "2.0";
        notif.method = method;
        notif.params = params;

        auto msg = MessageSerializer::serialize(notif);
        transport_->send(msg);
    }

    ServerOptions options_;
    std::unique_ptr<ITransport> transport_;
    RequestRouter router_;

    // Handlers
    InitializeHandler initialize_handler_;
    ToolsListHandler tools_list_handler_;
    ToolsCallHandler tools_call_handler_;
    ResourcesListHandler resources_list_handler_;
    ResourceReadHandler resource_read_handler_;
    PromptsListHandler prompts_list_handler_;
    PromptsGetHandler prompts_get_handler_;

    std::thread server_thread_;
    std::atomic<bool> running_;
};

/**
 * @brief Tool helper utilities
 */
namespace tools {

/**
 * @brief Create a string input schema
 * @return JSON Schema object for string input
 */
inline JsonValue make_string_schema() {
    return JsonValue::object({{"type", "string"}});
}

/**
 * @brief Create an object input schema
 * @param properties Schema properties
 * @return JSON Schema object
 */
inline JsonValue make_object_schema(const std::map<std::string, JsonValue>& properties) {
    JsonValue obj = JsonValue::object();
    for (const auto& p : properties) {
        obj["properties"][p.first] = p.second;
    }
    obj["required"] = JsonValue::array();
    return obj;
}

} // namespace tools

} // namespace mcpp
