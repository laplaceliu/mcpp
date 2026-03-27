/**
 * @file itransport.hpp
 * @brief Abstract transport layer interface
 */
#pragma once

#include <string>
#include <functional>
#include <memory>

namespace mcpp {

/**
 * @brief Abstract transport interface
 * @details Implementations provide message delivery mechanisms
 *         (e.g., stdio, HTTP/SSE, WebSocket)
 */
class ITransport {
public:
    virtual ~ITransport() = default;

    /**
     * @brief Start the transport
     * @return true if started successfully
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the transport
     */
    virtual void stop() = 0;

    /**
     * @brief Send a message
     * @param message JSON string to send
     * @return true if sent successfully
     */
    virtual bool send(const std::string& message) = 0;

    /**
     * @brief Set message received callback
     * @param handler Callback function invoked with received message
     */
    using MessageHandler = std::function<void(const std::string&)>;
    virtual void on_message(MessageHandler handler) = 0;

    /**
     * @brief Set error callback
     * @param handler Callback function invoked on errors
     */
    using ErrorHandler = std::function<void(const std::string&)>;
    virtual void on_error(ErrorHandler handler) = 0;

    /**
     * @brief Check if transport is connected
     * @return true if connected
     */
    virtual bool is_connected() const = 0;
};

} // namespace mcpp