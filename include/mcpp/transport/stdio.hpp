/**
 * @file stdio.hpp
 * @brief Stdio transport implementation
 */
#pragma once

#include "transport.hpp"
#include <thread>
#include <atomic>
#include <string>
#include <vector>

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
    StdioTransport();

    /**
     * @brief Destructor - ensures transport is stopped
     */
    ~StdioTransport() override;

    bool start() override;
    void stop() override;
    bool send(const std::string& message) override;

    void on_message(MessageHandler handler) override;
    void on_error(ErrorHandler handler) override;

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
    void read_loop();

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
    static std::string frame(const std::string& message);

    /**
     * @brief Decode framed messages from buffer
     * @param data Input data
     * @param len Length of input data
     * @return Vector of decoded messages
     */
    static std::vector<std::string> decode(const char* data, size_t len);

    /**
     * @brief Decode framed messages with remainder
     * @param data Input data
     * @param len Length of input data
     * @param remainder Unprocessed data (output)
     * @return Vector of decoded messages
     */
    static std::vector<std::string> decode(const char* data, size_t len, std::string& remainder);

private:
    static const char DELIMITER = '\n';
};

} // namespace mcpp
