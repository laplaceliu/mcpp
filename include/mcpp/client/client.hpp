/**
 * @file client.hpp
 * @brief MCP Client implementation
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
#include "mcpp/transport/transport.hpp"

namespace mcpp {

/**
 * @brief Client options
 */
struct ClientOptions {
    std::string name = "mcpp-client";          ///< Client name
    std::string version = "1.0.0";             ///< Client version
    std::string protocol_version = "2025-06-18"; ///< MCP protocol version

    ClientCapabilities capabilities;            ///< Client capabilities

    TransportFactory::Type transport_type = TransportFactory::Type::Stdio; ///< Transport type
    std::string host = "localhost";             ///< Server host (for HTTP)
    int port = 8080;                           ///< Server port (for HTTP)
};

/**
 * @brief Result of sampling request
 */
struct SamplingResult {
    bool is_error = false;                     ///< Whether the request resulted in error
    JsonValue content;                         ///< Generated content
    std::string error;                         ///< Error message if is_error is true
};

/**
 * @brief Result of elicitation request
 */
struct ElicitationResult {
    bool is_error = false;                     ///< Whether the request resulted in error
    JsonValue content;                         ///< User's response content
    std::string error;                         ///< Error message if is_error is true
};

/**
 * @brief Message content for prompts
 */
struct ContentBlock {
    std::string type;                          ///< Content type (text, image, audio)
    std::string text;                          ///< Text content (if type is "text")
    std::string mime_type;                     ///< MIME type for binary content
    std::string data;                          ///< Base64 encoded binary data
};

/**
 * @brief Sampling message for LLM interaction
 */
struct SamplingMessage {
    std::string role;                          ///< "user" or "assistant"
    std::vector<ContentBlock> content;         ///< Message content
};

/**
 * @brief Parameters for sampling request
 */
struct SamplingParams {
    std::string method;                        ///< Sampling method (e.g., "text/completion")
    std::vector<SamplingMessage> messages;    ///< Conversation messages
    std::string system_prompt;                 ///< System prompt
    JsonValue params;                          ///< Additional parameters
};

/**
 * @brief Parameters for elicitation request
 */
struct ElicitationParams {
    std::string message;                       ///< Message to present to user
    std::vector<ContentBlock> prefill;         ///< Prefill content
    std::vector<JsonValue> options;            ///< Response options
    bool requested_schema_is_partial = false; ///< Whether schema is partial
    JsonValue requested_schema;                ///< Schema for expected response
};

/**
 * @brief Main MCP Client class
 * @details Implements the Model Context Protocol client-side functionality
 * @note This class allows a server to request sampling, elicitation, and logging
 */
class Client : public std::enable_shared_from_this<Client> {
public:
    /**
     * @brief Construct a client
     * @param options Client configuration options
     */
    explicit Client(const ClientOptions& options = ClientOptions());

    /**
     * @brief Destructor - stops the client
     */
    ~Client();

    // ============ Lifecycle ============

    /**
     * @brief Start the client
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the client
     */
    void stop();

    /**
     * @brief Wait for client thread to complete
     */
    void wait();

    // ============ Sampling (Server -> Client) ============

    /**
     * @brief Send a sampling request to the client
     * @param params Sampling parameters
     * @param callback Callback with the result
     * @note This is called by the server to request LLM completion
     */
    void sampling_complete(const SamplingParams& params,
                          std::function<void(const SamplingResult&)> callback);

    // ============ Elicitation (Server -> Client) ============

    /**
     * @brief Send an elicitation request to the client
     * @param params Elicitation parameters
     * @param callback Callback with the result
     * @note This is called by the server to request user input
     */
    void elicitation_request(const ElicitationParams& params,
                            std::function<void(const ElicitationResult&)> callback);

    // ============ Logging (Server -> Client) ============

    /**
     * @brief Send a log message to the client
     * @param level Log level (debug, info, warning, error)
     * @param data Log data
     * @note This is called by the server to send log messages
     */
    void logging_message(const std::string& level, const JsonValue& data);

    // ============ Progress (Server -> Client) ============

    /**
     * @brief Send a progress notification to the client
     * @param progress Progress value (0.0 to 1.0)
     * @param total Total value
     * @param message Progress message
     */
    void progress_report(double progress, double total, const std::string& message);

    // ============ Client Callbacks (Set by Server) ============

    /**
     * @brief Set callback for sampling request received from server
     * @param handler Callback function
     */
    void on_sampling_request(std::function<SamplingResult(const SamplingParams&)> handler);

    /**
     * @brief Set callback for elicitation request received from server
     * @param handler Callback function
     */
    void on_elicitation_request(std::function<ElicitationResult(const ElicitationParams&)> handler);

    /**
     * @brief Set callback for progress notification from server
     * @param handler Callback function
     */
    void on_progress_notification(std::function<void(double, double, const std::string&)> handler);

    // ============ Connection State ============

    /**
     * @brief Check if client is connected
     * @return true if connected
     */
    bool is_connected() const;

    /**
     * @brief Get client capabilities
     * @return Client capabilities
     */
    const ClientCapabilities& capabilities() const { return options_.capabilities; }

private:
    void setup_handlers();
    void handle_message(const std::string& message);
    void handle_notification(const JsonRpcRequest& notification);
    void send_request(const std::string& method, const JsonValue& params,
                     std::function<void(const JsonRpcResponse&)> callback);

    ClientOptions options_;
    std::unique_ptr<ITransport> transport_;

    std::thread client_thread_;
    std::atomic<bool> running_;

    // Handlers set by server
    std::function<SamplingResult(const SamplingParams&)> sampling_handler_;
    std::function<ElicitationResult(const ElicitationParams&)> elicitation_handler_;
    std::function<void(double, double, const std::string&)> progress_handler_;
};

/**
 * @brief Helper utilities for creating content blocks
 */
namespace content {

/**
 * @brief Create a text content block
 * @param text Text content
 * @return ContentBlock for text
 */
inline ContentBlock make_text(const std::string& text) {
    ContentBlock block;
    block.type = "text";
    block.text = text;
    return block;
}

/**
 * @brief Create an image content block
 * @param data Base64 encoded image data
 * @param mime_type Image MIME type
 * @return ContentBlock for image
 */
inline ContentBlock make_image(const std::string& data, const std::string& mime_type) {
    ContentBlock block;
    block.type = "image";
    block.data = data;
    block.mime_type = mime_type;
    return block;
}

} // namespace content

} // namespace mcpp