/**
 * @file websocket.hpp
 * @brief WebSocket transport implementation using cpp-httplib
 */
#pragma once

#include "transport.hpp"
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <condition_variable>

namespace mcpp {

/**
 * @brief WebSocket transport configuration
 */
struct WebSocketConfig {
    std::string host = "localhost";      ///< Server host
    int port = 8080;                      ///< Server port
    std::string path = "/ws";             ///< WebSocket path
    bool use_ssl = false;                 ///< Use WSS (TLS)
    int connection_timeout_sec = 30;      ///< Connection timeout
    int ping_interval_sec = 30;          ///< Ping interval for keepalive
    int ping_timeout_sec = 10;            ///< Ping timeout
};

/**
 * @brief WebSocket transport for bidirectional communication
 * @details Uses cpp-httplib for WebSocket implementation
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

    std::thread worker_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;

    std::vector<std::string> message_queue_;
    std::atomic<bool> has_message_;

    bool use_ssl_ = false;

    void worker_loop();
    void process_messages();
    bool connect_client();
    void disconnect_client();
};

/**
 * @brief WebSocket message framing
 * @details MCP over WebSocket uses JSON messages with UTF-8 encoding
 */
class WebSocketFramer {
public:
    /**
     * @brief Frame a message for transmission
     * @param message Raw JSON message
     * @return The same message (WebSocket handles framing)
     */
    static std::string frame(const std::string& message);

    /**
     * @brief Check if message is complete
     * @param data Input data
     * @param len Length of input data
     * @return true if message is complete
     */
    static bool is_complete(const char* data, size_t len);
};

} // namespace mcpp
