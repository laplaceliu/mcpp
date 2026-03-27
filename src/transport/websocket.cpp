/**
 * @file websocket.cpp
 * @brief WebSocket transport implementation using libwebsockets
 */

#include "mcpp/transport/websocket.hpp"
#include "mcpp/core/error.hpp"

#include <cstring>
#include <random>

namespace mcpp {

// Per-session data structure
struct PerSessionData {
    WebSocketTransport* transport = nullptr;
    std::string client_id;
};

// libwebsockets protocol definition
static struct lws_protocols protocols[] = {
    {
        "mcp",
        websocket_callback,
        sizeof(PerSessionData),
        4096,
    },
    { nullptr, nullptr, 0, 0 }
};

// ============ WebSocketTransport ============

WebSocketTransport::WebSocketTransport(const WebSocketConfig& config)
    : config_(config),
      connected_(false),
      running_(false),
      use_ssl_(false),
      ctx_(nullptr),
      wsi_(nullptr) {
    // Build URL from config
    std::string scheme = config_.use_ssl ? "wss" : "ws";
    if (!config_.is_server) {
        url_ = scheme + "://" + config_.host + ":" +
               std::to_string(config_.port) + config_.path;
    } else {
        url_ = scheme + "://" + config_.host + ":" + std::to_string(config_.port) + config_.path;
    }
    use_ssl_ = config_.use_ssl;
}

WebSocketTransport::~WebSocketTransport() {
    stop();
}

void WebSocketTransport::set_url(const std::string& url) {
    url_ = url;
    use_ssl_ = false;
    config_.is_server = false;

    // Parse URL to update config
    std::string parsed_url = url;
    if (parsed_url.size() >= 6 && parsed_url.substr(0, 6) == "wss://") {
        use_ssl_ = true;
        parsed_url = parsed_url.substr(6);
    } else if (parsed_url.size() >= 5 && parsed_url.substr(0, 5) == "ws://") {
        use_ssl_ = false;
        parsed_url = parsed_url.substr(5);
    }

    // Extract host, port, path from parsed_url
    auto colon_pos = parsed_url.find(':');
    auto slash_pos = parsed_url.find('/');

    if (colon_pos != std::string::npos) {
        config_.host = parsed_url.substr(0, colon_pos);
        if (slash_pos != std::string::npos) {
            auto port_str = parsed_url.substr(colon_pos + 1, slash_pos - colon_pos - 1);
            config_.port = std::stoi(port_str);
            config_.path = parsed_url.substr(slash_pos);
        } else {
            config_.port = std::stoi(parsed_url.substr(colon_pos + 1));
            config_.path = "/";
        }
    } else if (slash_pos != std::string::npos) {
        config_.host = parsed_url.substr(0, slash_pos);
        config_.path = parsed_url.substr(slash_pos);
        config_.port = use_ssl_ ? 443 : 80;
    } else {
        config_.host = parsed_url;
        config_.path = "/";
        config_.port = use_ssl_ ? 443 : 80;
    }
}

int websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                      void* user, void* in, size_t len) {
    (void)len;

    // Get per-session data
    auto* pss = (PerSessionData*)user;
    WebSocketTransport* transport = pss ? pss->transport : nullptr;

    switch (reason) {
        // ===== Client callbacks =====
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            if (transport) {
                transport->connected_ = true;
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (transport && transport->message_handler_ && in) {
                std::string msg(static_cast<char*>(in), len);
                transport->message_handler_(msg);
            }
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (transport) {
                std::lock_guard<std::mutex> lock(transport->mutex_);
                if (!transport->message_queue_.empty()) {
                    const auto& msg = transport->message_queue_.front();

                    unsigned char buf[LWS_PRE + 4096];
                    unsigned char* p = &buf[LWS_PRE];

                    size_t n = msg.size();
                    if (n > sizeof(buf) - LWS_PRE) {
                        n = sizeof(buf) - LWS_PRE;
                    }

                    memcpy(p, msg.data(), n);

                    int written = lws_write(wsi, p, n, LWS_WRITE_TEXT);
                    if (written >= 0) {
                        transport->message_queue_.erase(transport->message_queue_.begin());
                    }
                }
            }
            break;

        // ===== Server callbacks =====
        case LWS_CALLBACK_ESTABLISHED:
            if (transport && pss) {
                pss->transport = transport;
                std::lock_guard<std::mutex> lock(transport->clients_mutex_);
                pss->client_id = "client_" + std::to_string(transport->next_client_id_++);
                transport->clients_[wsi] = pss->client_id;
                transport->connected_ = true;
            }
            break;

        case LWS_CALLBACK_RECEIVE:
            if (transport && transport->message_handler_ && in) {
                std::string msg(static_cast<char*>(in), len);
                transport->message_handler_(msg);
            }
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            if (transport) {
                std::lock_guard<std::mutex> lock(transport->mutex_);
                if (!transport->message_queue_.empty()) {
                    const auto& msg = transport->message_queue_.front();

                    unsigned char buf[LWS_PRE + 4096];
                    unsigned char* p = &buf[LWS_PRE];

                    size_t n = msg.size();
                    if (n > sizeof(buf) - LWS_PRE) {
                        n = sizeof(buf) - LWS_PRE;
                    }

                    memcpy(p, msg.data(), n);

                    int written = lws_write(wsi, p, n, LWS_WRITE_TEXT);
                    if (written >= 0) {
                        transport->message_queue_.erase(transport->message_queue_.begin());
                    }
                }
            }
            break;

        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_WSI_DESTROY:
            if (transport) {
                std::lock_guard<std::mutex> lock(transport->clients_mutex_);
                transport->clients_.erase(wsi);
                if (transport->clients_.empty()) {
                    transport->connected_ = false;
                }
            }
            break;

        case LWS_CALLBACK_CONNECTING:
            break;

        default:
            break;
    }

    return 0;
}

bool WebSocketTransport::start() {
    if (running_) {
        return true;
    }

    running_ = true;
    connected_ = false;

    // Create lws context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));

    info.port = config_.is_server ? config_.port : CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    if (config_.is_server) {
        // Server mode
        if (config_.use_ssl) {
            info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        }
        info.iface = config_.host.empty() ? nullptr : config_.host.c_str();
    }

    ctx_ = lws_create_context(&info);
    if (!ctx_) {
        if (error_handler_) {
            error_handler_(config_.is_server ? "Failed to create WebSocket server" : "Failed to create libwebsockets context");
        }
        running_ = false;
        return false;
    }

    if (!config_.is_server) {
        // Client mode - connect to server
        struct lws_client_connect_info ccinfo;
        memset(&ccinfo, 0, sizeof(ccinfo));

        ccinfo.context = ctx_;
        ccinfo.address = config_.host.c_str();
        ccinfo.port = config_.port;
        ccinfo.path = config_.path.c_str();
        ccinfo.host = ccinfo.address;
        ccinfo.origin = ccinfo.address;
        ccinfo.protocol = "mcp";
        ccinfo.pwsi = &wsi_;

        if (use_ssl_) {
            ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_INSECURE;
        } else {
            ccinfo.ssl_connection = 0;
        }

        wsi_ = lws_client_connect_via_info(&ccinfo);
        if (!wsi_) {
            if (error_handler_) {
                error_handler_("Failed to connect to " + url_);
            }
            lws_context_destroy(ctx_);
            ctx_ = nullptr;
            running_ = false;
            return false;
        }

        // Set user data to point back to transport
        auto* pss = (PerSessionData*)lws_wsi_user(wsi_);
        if (pss) {
            pss->transport = this;
        }
    }

    // Start service thread
    service_thread_ = std::thread(&WebSocketTransport::service_loop, this);

    return true;
}

void WebSocketTransport::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (service_thread_.joinable()) {
        service_thread_.join();
    }

    if (ctx_) {
        lws_context_destroy(ctx_);
        ctx_ = nullptr;
    }

    wsi_ = nullptr;
    connected_ = false;
    clients_.clear();
}

bool WebSocketTransport::send(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.is_server) {
        // Client mode - single connection
        if (!connected_) {
            return false;
        }
        message_queue_.push_back(message);
        if (wsi_) {
            lws_callback_on_writable(wsi_);
        }
    } else {
        // Server mode - broadcast to all clients
        if (clients_.empty()) {
            return false;
        }
        message_queue_.push_back(message);
        // Request callback for all connections to send
        std::lock_guard<std::mutex> clients_lock(clients_mutex_);
        for (auto& client : clients_) {
            lws_callback_on_writable(client.first);
        }
    }

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

void WebSocketTransport::service_loop() {
    while (running_ && ctx_) {
        // Service libwebsockets
        int n = lws_service(ctx_, 50);
        if (n < 0) {
            break;
        }
    }
}

// ============ WebSocketFramer ============

std::string WebSocketFramer::frame(const std::string& message) {
    std::string frame;
    frame.reserve(message.size() + 10);

    // Byte 0: FIN + text opcode (0x81)
    frame += static_cast<char>(0x81);

    // Byte 1: Mask bit + payload length
    uint8_t mask_bit = 0x80;  // Set mask bit for client sending

    if (message.size() <= 125) {
        frame += static_cast<char>(mask_bit | message.size());
    } else if (message.size() <= 65535) {
        frame += static_cast<char>(mask_bit | 126);
        frame += static_cast<char>((message.size() >> 8) & 0xFF);
        frame += static_cast<char>(message.size() & 0xFF);
    } else {
        frame += static_cast<char>(mask_bit | 127);
        for (int i = 7; i >= 0; i--) {
            frame += static_cast<char>((message.size() >> (i * 8)) & 0xFF);
        }
    }

    // Generate and append masking key
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    char mask_key[4];
    for (int i = 0; i < 4; i++) {
        mask_key[i] = static_cast<char>(dis(gen));
        frame += mask_key[i];
    }

    // Append masked payload
    for (size_t i = 0; i < message.size(); i++) {
        frame += message[i] ^ mask_key[i % 4];
    }

    return frame;
}

bool WebSocketFramer::is_complete(const char* data, size_t len) {
    if (len < 2) {
        return false;
    }

    uint8_t first_byte = static_cast<uint8_t>(data[0]);
    uint8_t opcode = first_byte & 0x0F;

    uint64_t payload_len = static_cast<uint8_t>(data[1]) & 0x7F;
    bool masked = (static_cast<uint8_t>(data[1]) & 0x80) != 0;

    size_t header_len = 2;

    if (payload_len == 126) {
        if (len < 4) return false;
        payload_len = (static_cast<uint8_t>(data[2]) << 8) |
                      static_cast<uint8_t>(data[3]);
        header_len += 2;
    } else if (payload_len == 127) {
        if (len < 10) return false;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | static_cast<uint8_t>(data[2 + i]);
        }
        header_len += 8;
    }

    if (masked) {
        header_len += 4;
    }

    return len >= (header_len + payload_len);
}

bool WebSocketFramer::unmask_payload(const char* masked_data, size_t masked_len,
                                      char* unmasked_data, size_t unmasked_len,
                                      const char* mask_key) {
    if (unmasked_len != masked_len) {
        return false;
    }

    for (size_t i = 0; i < masked_len; i++) {
        unmasked_data[i] = masked_data[i] ^ mask_key[i % 4];
    }

    return true;
}

} // namespace mcpp