/**
 * @file stdio.hpp
 * @brief Stdio transport implementation
 */
#pragma once

#include "itransport.hpp"
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>

namespace mcpp {

/**
 * @brief Stdio transport for local process communication
 * @details Uses stdin/stdout for message passing
 */
class StdioTransport : public ITransport {
public:
    /**
     * @brief Construct a StdioTransport
     */
    StdioTransport() : running_(false), read_from_stdin_(true) {}

    /**
     * @brief Destructor - ensures transport is stopped
     */
    ~StdioTransport() override {
        stop();
    }

    bool start() override {
        if (running_) return true;
        running_ = true;
        read_thread_ = std::thread(&StdioTransport::read_loop, this);
        return true;
    }

    void stop() override {
        if (!running_) return;
        running_ = false;
        if (read_thread_.joinable()) {
            read_thread_.join();
        }
    }

    bool send(const std::string& message) override {
        std::cout << message << "\n" << std::flush;
        return true;
    }

    void on_message(MessageHandler handler) override {
        message_handler_ = std::move(handler);
    }

    void on_error(ErrorHandler handler) override {
        error_handler_ = std::move(handler);
    }

    bool is_connected() const override { return running_; }

    /**
     * @brief Configure whether to read from stdin
     * @param value true to read from stdin (default), false otherwise
     */
    void set_read_from_stdin(bool value) { read_from_stdin_ = value; }

private:
    /**
     * @brief Main read loop
     */
    void read_loop() {
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

    std::thread read_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> read_from_stdin_;

    MessageHandler message_handler_;
    ErrorHandler error_handler_;

    std::string buffer_;
};

/**
 * @brief Message framing for stdio transport
 */
class StdioFramer {
public:
    /**
     * @brief Frame a message for transmission
     * @param message Raw message
     * @return Framed message with delimiter
     */
    static std::string frame(const std::string& message) {
        return message + DELIMITER;
    }

    /**
     * @brief Decode framed messages from buffer
     * @param data Input data
     * @param len Length of input data
     * @return Vector of decoded messages
     */
    static std::vector<std::string> decode(const char* data, size_t len) {
        std::string remainder;
        return decode(data, len, remainder);
    }

    /**
     * @brief Decode framed messages with remainder
     * @param data Input data
     * @param len Length of input data
     * @param remainder Unprocessed data (output)
     * @return Vector of decoded messages
     */
    static std::vector<std::string> decode(const char* data, size_t len, std::string& remainder) {
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

private:
    static const char DELIMITER = '\n';
};

} // namespace mcpp
