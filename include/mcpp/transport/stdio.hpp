/**
 * @file stdio.hpp
 * @brief Stdio transport implementation for MCP
 * @details Uses stdin/stdout for JSON-RPC message passing
 *          Reference: gopher-mcp/examples/stdio_echo implementation
 */
#pragma once

#include "itransport.hpp"
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <iostream>
#include <cstring>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

namespace mcpp {

/**
 * @brief Stdio transport for local process communication
 * @details Uses stdin for input and stdout for output.
 *          Uses poll() for efficient waiting with timeout.
 *          stdout is reserved exclusively for JSON-RPC messages.
 */
class StdioTransport : public ITransport {
public:
    StdioTransport() : running_(false) {}

    ~StdioTransport() override {
        stop();
    }

    bool start() override {
        if (running_.exchange(true)) {
            return true;
        }

        // Set stdin to non-blocking mode
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags != -1) {
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        }

        // Start the read thread
        read_thread_ = std::thread(&StdioTransport::read_loop, this);

        return true;
    }

    void stop() override {
        if (!running_.exchange(false)) {
            return;
        }

        // Wait for the read thread to finish
        if (read_thread_.joinable()) {
            read_thread_.join();
        }
    }

    bool send(const std::string& message) override {
        if (!running_) {
            return false;
        }

        std::lock_guard<std::mutex> lock(write_mutex_);

        // Write message and newline to stdout
        std::cout << message << "\n";
        std::cout.flush();

        return true;
    }

    void on_message(MessageHandler handler) override {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        message_handler_ = std::move(handler);
    }

    void on_error(ErrorHandler handler) override {
        std::lock_guard<std::mutex> lock(handler_mutex_);
        error_handler_ = std::move(handler);
    }

    bool is_connected() const override { return running_; }

private:
    void read_loop() {
        char buffer[8192];
        std::string line_buffer;

        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;

        while (running_) {
            // Poll with 100ms timeout for responsive shutdown
            int ret = poll(&pfd, 1, 100);

            if (ret > 0 && (pfd.revents & POLLIN)) {
                ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));

                if (n > 0) {
                    // Data received - append to line buffer
                    line_buffer.append(buffer, n);

                    // Process complete lines
                    size_t pos;
                    while ((pos = line_buffer.find('\n')) != std::string::npos) {
                        std::string line = line_buffer.substr(0, pos);
                        line_buffer.erase(0, pos + 1);

                        if (!line.empty()) {
                            process_message(line);
                        }
                    }
                } else if (n == 0) {
                    // EOF - stdin closed
                    // For MCP, we continue running and wait for reconnection
                    // Just break out and let the loop check running_ flag
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                // n < 0 is EAGAIN/EWOULDBLOCK for non-blocking, ignore
            }
        }
    }

    void process_message(const std::string& message) {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (message_handler_) {
            try {
                message_handler_(message);
            } catch (const std::exception& e) {
                if (error_handler_) {
                    error_handler_(std::string("Handler error: ") + e.what());
                }
            }
        }
    }

    std::thread read_thread_;
    std::atomic<bool> running_;

    std::mutex write_mutex_;
    std::mutex handler_mutex_;

    MessageHandler message_handler_;
    ErrorHandler error_handler_;
};

} // namespace mcpp
