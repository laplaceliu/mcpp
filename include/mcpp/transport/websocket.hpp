/**
 * @file websocket.hpp
 * @brief WebSocket transport implementation using libwebsockets
 */
#pragma once

#include "transport.hpp"
#include <libwebsockets.h>

#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <map>

namespace mcpp {

/**
 * @brief WebSocket transport configuration
 */
struct WebSocketConfig {
    std::string host = "localhost";      ///< Server host (client) or bind address (server)
    int port = 8080;                     ///< Server port
    std::string path = "/ws";            ///< WebSocket path
    bool use_ssl = false;                ///< Use WSS (TLS)
    bool is_server = false;             ///< Run as server instead of client
    int connection_timeout_sec = 30;     ///< Connection timeout
    int ping_interval_sec = 30;          ///< Ping interval for keepalive
    int ping_timeout_sec = 10;           ///< Ping timeout
};

/**
 * @brief WebSocket transport for bidirectional communication
 * @details Provides WebSocket client transport for MCP using libwebsockets
 */
class WebSocketTransport : public ITransport {
public:
    /**
     * @brief Construct a WebSocketTransport
     * @param config WebSocket configuration
     */
    explicit WebSocketTransport(const WebSocketConfig& config = WebSocketConfig());

    /**
     * @brief Destructor - ensures transport is stopped
     */
    ~WebSocketTransport() override;

    /**
     * @brief Set connection URL (alternative to config)
     * @param url Full WebSocket URL (e.g., "ws://localhost:8080/ws")
     */
    void set_url(const std::string& url);

    /**
     * @brief Connect as client
     * @return true if connection successful
     */
    bool start() override;

    /**
     * @brief Disconnect
     */
    void stop() override;

    /**
     * @brief Send a message
     * @param message JSON string to send
     * @return true if sent successfully
     */
    bool send(const std::string& message) override;

    /**
     * @brief Set message received callback
     * @param handler Callback function invoked with received message
     */
    void on_message(MessageHandler handler) override;

    /**
     * @brief Set error callback
     * @param handler Callback function invoked on errors
     */
    void on_error(ErrorHandler handler) override;

    /**
     * @brief Check if connected
     * @return true if WebSocket is connected
     */
    bool is_connected() const override;

    /**
     * @brief Get connection URL
     * @return Current WebSocket URL
     */
    std::string url() const { return url_; }

private:
    WebSocketConfig config_;
    std::string url_;
    std::atomic<bool> connected_;
    std::atomic<bool> running_;

    MessageHandler message_handler_;
    ErrorHandler error_handler_;

    std::mutex mutex_;
    std::vector<std::string> message_queue_;

    bool use_ssl_ = false;

    // libwebsockets context and connection
    struct lws_context* ctx_ = nullptr;
    struct lws* wsi_ = nullptr;
    std::thread service_thread_;

    // Server mode: track all connections
    std::mutex clients_mutex_;
    std::map<struct lws*, std::string> clients_;  // wsi -> client_id
    int next_client_id_ = 0;

    void service_loop();

    friend int websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                 void* user, void* in, size_t len);
};

// External callback for libwebsockets
int websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                      void* user, void* in, size_t len);

/**
 * @brief WebSocket message framing utilities
 * @details MCP over WebSocket uses JSON messages with UTF-8 encoding
 *
 * Implements RFC 6455 WebSocket framing format
 */
class WebSocketFramer {
public:
    /**
     * @brief Frame a WebSocket text message
     * @param message Raw JSON message payload
     * @return Complete WebSocket frame ready for transmission
     */
    static std::string frame(const std::string& message);

    /**
     * @brief Check if a WebSocket frame is complete
     * @param data Input data buffer
     * @param len Length of input data
     * @return true if the frame is complete and can be processed
     */
    static bool is_complete(const char* data, size_t len);

    /**
     * @brief Unmask WebSocket payload data
     * @param masked_data Input masked data
     * @param masked_len Length of masked data
     * @param unmasked_data Output buffer for unmasked data
     * @param unmasked_len Length of unmasked buffer
     * @param mask_key 4-byte masking key
     * @return true if successful
     */
    static bool unmask_payload(const char* masked_data, size_t masked_len,
                               char* unmasked_data, size_t unmasked_len,
                               const char* mask_key);
};

} // namespace mcpp