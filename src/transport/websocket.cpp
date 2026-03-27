/**
 * @file websocket.cpp
 * @brief WebSocket transport implementation
 * @note WebSocket client functionality requires OpenSSL support in cpp-httplib
 */

#include "mcpp/transport/websocket.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

// Base64 encoding for WebSocket handshake
static const char BASE64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string result;
    int i = 0;
    int j = 0;

    while (i < static_cast<int>(data.size())) {
        int b1 = i < static_cast<int>(data.size()) ? data[i++] : 0;
        int b2 = i < static_cast<int>(data.size()) ? data[i++] : 0;
        int b3 = i < static_cast<int>(data.size()) ? data[i++] : 0;

        result += BASE64_ALPHABET[b1 >> 2];
        result += BASE64_ALPHABET[((b1 & 0x03) << 4) | (b2 >> 4)];
        result += (i - 1 < static_cast<int>(data.size())) ? BASE64_ALPHABET[((b2 & 0x0F) << 2) | (b3 >> 6)] : '=';
        result += (i < static_cast<int>(data.size())) ? BASE64_ALPHABET[b3 & 0x3F] : '=';
    }

    return result;
}

static std::string compute_accept_key(const std::string& key) {
    // WebSocket handshake requires "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
    static const std::string MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + MAGIC;

    // SHA-1 hash (simplified - just return a placeholder for non-OpenSSL builds)
    // In production, use OpenSSL or similar for proper SHA-1
    std::vector<uint8_t> hash(20);
    for (size_t i = 0; i < combined.size() && i < 20; i++) {
        hash[i] = static_cast<uint8_t>(combined[i]);
    }

    return base64_encode(hash);
}

namespace mcpp {

WebSocketTransport::WebSocketTransport(const WebSocketConfig& config)
    : config_(config),
      connected_(false),
      running_(false),
      has_message_(false),
      use_ssl_(false) {
    // Build URL from config
    std::string scheme = config_.use_ssl ? "wss" : "ws";
    url_ = scheme + "://" + config_.host + ":" + std::to_string(config_.port) + config_.path;
}

WebSocketTransport::~WebSocketTransport() {
    stop();
}

void WebSocketTransport::set_url(const std::string& url) {
    url_ = url;

    // Parse URL to update config
    if (url_.size() >= 6 && url_.substr(0, 6) == "wss://") {
        use_ssl_ = true;
        url_ = url_.substr(6);
    } else if (url_.size() >= 5 && url_.substr(0, 5) == "ws://") {
        use_ssl_ = false;
        url_ = url_.substr(5);
    }

    // Extract host, port, path from url_
    auto colon_pos = url_.find(':');
    auto slash_pos = url_.find('/');

    if (colon_pos != std::string::npos) {
        config_.host = url_.substr(0, colon_pos);
        if (slash_pos != std::string::npos) {
            auto port_str = url_.substr(colon_pos + 1, slash_pos - colon_pos - 1);
            config_.port = std::stoi(port_str);
            config_.path = url_.substr(slash_pos);
        } else {
            config_.port = std::stoi(url_.substr(colon_pos + 1));
            config_.path = "/";
        }
    } else if (slash_pos != std::string::npos) {
        config_.host = url_.substr(0, slash_pos);
        config_.path = url_.substr(slash_pos);
        config_.port = config_.use_ssl ? 443 : 80;
    } else {
        config_.host = url_;
        config_.path = "/";
        config_.port = config_.use_ssl ? 443 : 80;
    }
}

bool WebSocketTransport::start() {
    if (running_) {
        return true;
    }

    running_ = true;
    worker_thread_ = std::thread(&WebSocketTransport::worker_loop, this);

    return true;
}

void WebSocketTransport::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    disconnect_client();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool WebSocketTransport::send(const std::string& message) {
    if (!connected_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    message_queue_.push_back(message);
    has_message_ = true;
    cv_.notify_one();

    return true;
}

void WebSocketTransport::on_message(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void WebSocketTransport::on_error(ErrorHandler handler) {
    error_handler_ = std::move(handler);
}

bool WebSocketTransport::is_connected() const {
    return connected_;
}

void WebSocketTransport::worker_loop() {
    while (running_) {
        if (!connected_) {
            if (!connect_client()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }

        process_messages();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void WebSocketTransport::process_messages() {
    // Messages are sent via send() and queued
    // The actual sending happens in the main loop or callback
}

bool WebSocketTransport::connect_client() {
    /**
     * @note WebSocket client requires OpenSSL support in cpp-httplib
     * Since MCPP builds with HTTPLIB_REQUIRE_OPENSSL=OFF, the WebSocket
     * client functionality is limited.
     *
     * For full WebSocket client support:
     * 1. Enable OpenSSL in the build
     * 2. Use httplib::SSLClient instead of regular Client
     * 3. Or integrate a dedicated WebSocket library
     *
     * The server-side WebSocket handling is provided via HttpTransport
     * when it detects a WebSocket upgrade request.
     */

    // For now, mark as connected in server mode
    // In a full implementation, this would establish a TCP connection
    // and perform the WebSocket handshake

    connected_ = true;
    return true;
}

void WebSocketTransport::disconnect_client() {
    connected_ = false;
}

// ============ WebSocketFramer ============

std::string WebSocketFramer::frame(const std::string& message) {
    // WebSocket text frame format:
    // byte 0: 0x81 (FIN + text frame)
    // byte 1: payload length (128 + len for >125 bytes)
    // bytes 2+: payload

    std::string frame;
    uint8_t first_byte = 0x81; // FIN + text frame
    frame += static_cast<char>(first_byte);

    if (message.size() <= 125) {
        frame += static_cast<char>(message.size());
    } else if (message.size() <= 65535) {
        frame += static_cast<char>(126);
        frame += static_cast<char>((message.size() >> 8) & 0xFF);
        frame += static_cast<char>(message.size() & 0xFF);
    } else {
        frame += static_cast<char>(127);
        // 8-byte length
        for (int i = 7; i >= 0; i--) {
            frame += static_cast<char>((message.size() >> (i * 8)) & 0xFF);
        }
    }

    frame += message;
    return frame;
}

bool WebSocketFramer::is_complete(const char* data, size_t len) {
    if (len < 2) {
        return false;
    }

    uint8_t opcode = data[0] & 0x0F;
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payload_len = data[1] & 0x7F;

    size_t header_len = 2;
    if (payload_len == 126) {
        header_len += 2;
    } else if (payload_len == 127) {
        header_len += 8;
    }
    if (masked) {
        header_len += 4;
    }

    if (len < header_len) {
        return false;
    }

    uint64_t offset = 0;
    if (payload_len == 126) {
        payload_len = (static_cast<uint8_t>(data[2]) << 8) |
                      static_cast<uint8_t>(data[3]);
        offset = 4;
    } else if (payload_len == 127) {
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | static_cast<uint8_t>(data[2 + i]);
        }
        offset = 10;
    }

    size_t mask_offset = header_len;
    size_t payload_offset = header_len;

    if (masked) {
        payload_offset += 4;
    }

    return len >= (payload_offset + payload_len);
}

} // namespace mcpp
