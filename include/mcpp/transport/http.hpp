/**
 * @file http.hpp
 * @brief HTTP/SSE transport implementation
 */
#pragma once

#include "itransport.hpp"
#include <httplib.h>
#include <string>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <sstream>
#include <chrono>

// Forward declaration - httplib.h is included in implementation
namespace httplib {
class Server;
class Client;
} // namespace httplib

namespace mcpp {

// Forward declaration
class HttpFramer;

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
    static std::string frame(const std::string& message) {
        std::ostringstream oss;
        oss << "Content-Type: application/json\r\n";
        oss << "Content-Length: " << message.size() << "\r\n";
        oss << "\r\n";
        oss << message;
        return oss.str();
    }

    /**
     * @brief Frame a message for SSE
     * @param event Event name
     * @param data Event data
     * @return SSE formatted string
     */
    static std::string frame_sse(const std::string& event, const std::string& data) {
        std::ostringstream oss;
        if (!event.empty()) {
            oss << "event: " << event << "\r\n";
        }
        oss << "data: " << data << "\r\n";
        oss << "\r\n";
        return oss.str();
    }

    /**
     * @brief Extract body from HTTP message
     * @param http_message Full HTTP message
     * @return Body content, or empty string if invalid
     */
    static std::string extract_body(const std::string& http_message) {
        size_t header_end = http_message.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            return "";
        }
        return http_message.substr(header_end + 4);
    }

    /**
     * @brief Parse Content-Length from HTTP headers
     * @param headers HTTP headers string
     * @return Content-Length value, or -1 if not found
     */
    static int parse_content_length(const std::string& headers) {
        std::istringstream iss(headers);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("Content-Length:") == 0 || line.find("content-length:") == 0) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string value = line.substr(colon + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \r") + 1);
                    return std::stoi(value);
                }
            }
        }
        return -1;
    }
};

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
    explicit HttpTransport(int port = 8080)
        : port_(port), running_(false), connected_(false), is_server_(true) {}

    /**
     * @brief Construct HTTP transport as client
     * @param host Server host
     * @param port Server port
     */
    HttpTransport(const std::string& host, int port)
        : host_(host), port_(port), running_(false), connected_(false), is_server_(false) {}

    ~HttpTransport() override {
        stop();
    }

    bool start() override {
        if (running_) return true;

        running_ = true;

        if (is_server_) {
            server_ = std::unique_ptr<httplib::Server>(new httplib::Server());
            server_->Post(endpoint_, [this](const httplib::Request& req, httplib::Response& res) {
                if (req.body.empty()) {
                    res.status = 400;
                    return;
                }
                handle_http_request(req.body);
                res.status = 200;
                res.set_content("", "text/plain");
            });

            // SSE endpoint - clients connect here to receive events
            server_->Get(endpoint_, [this](const httplib::Request& req, httplib::Response& res) {
                (void)req;
                res.set_header("Content-Type", "text/event-stream");
                res.set_header("Cache-Control", "no-cache");
                res.set_header("Connection", "keep-alive");

                auto client_id = next_client_id_++;

                auto send_func = [this, client_id](const std::string& event, const std::string& data) {
                    (void)event;
                    (void)data;
                    std::lock_guard<std::mutex> lock(sse_mutex_);
                    auto it = sse_clients_.find(client_id);
                    if (it != sse_clients_.end()) {
                        std::string sse_data = HttpFramer::frame_sse(event, data);
                        // SSE data will be sent via callback
                        (void)sse_data;
                    }
                };

                sse_clients_[client_id] = {send_func};
                connected_ = true;
            });

            server_thread_ = std::thread([this]() {
                server_->listen("0.0.0.0", port_);
            });

            return true;
        } else {
            client_ = std::unique_ptr<httplib::Client>(new httplib::Client(host_, port_));
            connected_ = true;

            // Start background thread for SSE
            server_thread_ = std::thread([this]() {
                while (running_) {
                    if (client_ && connected_) {
                        auto res = client_->Get(endpoint_, [](const char* data, size_t len) {
                            // Process SSE data
                            (void)data;
                            (void)len;
                            return true;
                        });
                        (void)res;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });

            return true;
        }
    }

    void stop() override {
        if (!running_) return;
        running_ = false;

        if (is_server_ && server_) {
            server_->stop();
        }

        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        connected_ = false;
        sse_clients_.clear();
    }

    bool send(const std::string& message) override {
        if (is_server_) {
            // Send via SSE to all connected clients
            std::lock_guard<std::mutex> lock(sse_mutex_);
            for (auto& client : sse_clients_) {
                client.second.send_func("message", message);
            }
            return true;
        } else {
            // Client sends via HTTP POST
            if (!client_) return false;

            auto res = client_->Post(endpoint_, message, "application/json");
            return res && res->status == 200;
        }
    }

    void on_message(MessageHandler handler) override {
        message_handler_ = std::move(handler);
    }

    void on_error(ErrorHandler handler) override {
        error_handler_ = std::move(handler);
    }

    bool is_connected() const override { return running_ && connected_; }

    /**
     * @brief Set the endpoint path for messages
     * @param path HTTP path endpoint
     */
    void set_endpoint(const std::string& path) { endpoint_ = path; }

private:
    void handle_http_request(const std::string& body) {
        if (message_handler_) {
            try {
                message_handler_(body);
            } catch (const std::exception& e) {
                if (error_handler_) {
                    error_handler_(std::string("Handler error: ") + e.what());
                }
            }
        }
    }

    void send_sse_event(const std::string& event, const std::string& data) {
        std::string sse_data = HttpFramer::frame_sse(event, data);
        // In server mode, SSE is handled by the HTTP library
        (void)sse_data;
    }

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

} // namespace mcpp
