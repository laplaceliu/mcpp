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
    explicit Server(const ServerOptions& options = ServerOptions());

    /**
     * @brief Destructor - stops the server
     */
    ~Server();

    // ============ Lifecycle ============

    /**
     * @brief Start the server
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Wait for server thread to complete
     */
    void wait();

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
                       std::function<CallToolResult(const std::string&, const JsonValue&)> handler);

    /**
     * @brief Get list of registered tools
     * @return Vector of registered tools
     */
    std::vector<Tool> list_tools() const;

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
                           std::function<JsonValue()> handler);

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
                                    const std::string& mime_type);

    // ============ Prompt Registration ============

    /**
     * @brief Register a prompt template
     * @param name Prompt name
     * @param description Prompt description
     * @param handler Function to generate prompt
     */
    void register_prompt(const std::string& name,
                         const std::string& description,
                         std::function<GetPromptResult(const JsonValue&)> handler);

    // ============ Notifications ============

    /**
     * @brief Send tools changed notification
     */
    void notify_tools_changed();

    /**
     * @brief Send resources changed notification
     */
    void notify_resources_changed();

    /**
     * @brief Send resource updated notification
     * @param uri URI of updated resource
     */
    void notify_resource_updated(const std::string& uri);

    /**
     * @brief Send a log message
     * @param level Log level
     * @param data Log data
     */
    void log(const std::string& level, const JsonValue& data);

private:
    void setup_handlers();
    void handle_message(const std::string& message);
    void handle_request(const JsonRpcRequest& request, JsonRpcResponse& response);
    void send_notification(const std::string& method, const JsonValue& params);

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
