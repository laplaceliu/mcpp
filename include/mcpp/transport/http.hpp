/**
 * @file http.hpp
 * @brief HTTP/SSE transport implementation
 */
#pragma once

#include "transport.hpp"
#include <string>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

// Forward declaration - httplib.h is included in implementation
namespace httplib {
class Server;
class Client;
} // namespace httplib

namespace mcpp {

/**
 * @brief HTTP/SSE transport for remote communication
 * @details Uses HTTP POST for sending and SSE for receiving
 */
class HttpTransport : public ITransport {
public:
    /**
     * @brief Construct HTTP transport
     * @param port Server port (for server mode)
     */
    explicit HttpTransport(int port = 8080);

    /**
     * @brief Construct HTTP transport as client
     * @param host Server host
     * @param port Server port
     */
    HttpTransport(const std::string& host, int port);

    ~HttpTransport() override;

    bool start() override;
    void stop() override;
    bool send(const std::string& message) override;

    void on_message(MessageHandler handler) override;
    void on_error(ErrorHandler handler) override;

    bool is_connected() const override { return running_ && connected_; }

    /**
     * @brief Set the endpoint path for messages
     * @param path HTTP path endpoint
     */
    void set_endpoint(const std::string& path) { endpoint_ = path; }

private:
    void server_loop();
    void handle_http_request(const std::string& body);
    void send_sse_event(const std::string& event, const std::string& data);

    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<httplib::Client> client_;

    int port_;
    std::string host_;
    std::string endpoint_ = "/mcp";

    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::atomic<bool> is_server_;

    std::thread server_thread_;
    std::mutex sse_mutex_;
    std::condition_variable sse_cv_;

    MessageHandler message_handler_;
    ErrorHandler error_handler_;

    // SSE client info
    struct SseClient {
        std::function<void(const std::string&, const std::string&)> send_func;
    };
    std::map<int, SseClient> sse_clients_;
    int next_client_id_ = 0;
};

/**
 * @brief HTTP message framer
 * @details Uses Content-Length header for message framing
 */
class HttpFramer {
public:
    /**
     * @brief Frame a message with Content-Length header
     * @param message Message body
     * @return Full HTTP message with headers
     */
    static std::string frame(const std::string& message);

    /**
     * @brief Frame a message for SSE
     * @param event Event name
     * @param data Event data
     * @return SSE formatted string
     */
    static std::string frame_sse(const std::string& event, const std::string& data);

    /**
     * @brief Extract body from HTTP message
     * @param http_message Full HTTP message
     * @return Body content, or empty string if invalid
     */
    static std::string extract_body(const std::string& http_message);

    /**
     * @brief Parse Content-Length from HTTP headers
     * @param headers HTTP headers string
     * @return Content-Length value, or -1 if not found
     */
    static int parse_content_length(const std::string& headers);
};

} // namespace mcpp
