#include "mcpp/transport/stdio.hpp"
#include <iostream>
#include <cstring>

namespace mcpp {

StdioTransport::StdioTransport()
    : running_(false), read_from_stdin_(true) {
}

StdioTransport::~StdioTransport() {
    stop();
}

bool StdioTransport::start() {
    if (running_) return true;
    running_ = true;
    read_thread_ = std::thread(&StdioTransport::read_loop, this);
    return true;
}

void StdioTransport::stop() {
    if (!running_) return;
    running_ = false;
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
}

bool StdioTransport::send(const std::string& message) {
    std::cout << message << "\n" << std::flush;
    return true;
}

void StdioTransport::on_message(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void StdioTransport::on_error(ErrorHandler handler) {
    error_handler_ = std::move(handler);
}

void StdioTransport::read_loop() {
    std::string buffer;
    buffer.reserve(4096);

    while (running_) {
        char ch;
        if (std::cin.get(ch)) {
            if (ch == '\n') {
                // Line ending, process message
                if (!buffer.empty() && message_handler_) {
                    try {
                        message_handler_(buffer);
                    } catch (const std::exception& e) {
                        if (error_handler_) {
                            error_handler_(std::string("Handler error: ") + e.what());
                        }
                    }
                }
                buffer.clear();
            } else {
                buffer.push_back(ch);
            }
        } else {
            // Input stream ended
            break;
        }
    }
}

// ============ StdioFramer ============

std::string StdioFramer::frame(const std::string& message) {
    return message + DELIMITER;
}

std::vector<std::string> StdioFramer::decode(const char* data, size_t len) {
    std::string remainder;
    return decode(data, len, remainder);
}

std::vector<std::string> StdioFramer::decode(const char* data, size_t len, std::string& remainder) {
    std::vector<std::string> messages;

    // Append new data to remainder
    remainder.append(data, len);

    // Find delimiter
    size_t pos;
    while ((pos = remainder.find(DELIMITER)) != std::string::npos) {
        std::string msg = remainder.substr(0, pos);
        remainder.erase(0, pos + 1);
        if (!msg.empty()) {
            messages.push_back(msg);
        }
    }

    return messages;
}

} // namespace mcpp
