/**
 * @file factory.hpp
 * @brief Transport factory for creating transport instances
 */
#pragma once

#include "itransport.hpp"
#include "stdio.hpp"
#include "http.hpp"
#include "websocket.hpp"

#include <string>
#include <memory>

namespace mcpp {

/**
 * @brief Factory for creating transport instances
 */
class TransportFactory {
public:
    /**
     * @brief Transport type enumeration
     */
    enum class Type {
        Stdio,     ///< Standard I/O transport
        Http,      ///< HTTP/SSE transport
        WebSocket  ///< WebSocket transport
    };

    /**
     * @brief Create a transport by type
     * @param type Transport type to create
     * @return Unique pointer to transport, or nullptr if type not supported
     */
    static std::unique_ptr<ITransport> create(Type type) {
        switch (type) {
            case Type::Stdio:
                return std::unique_ptr<ITransport>(new StdioTransport());
            case Type::Http:
                return std::unique_ptr<ITransport>(new HttpTransport(8080));
            case Type::WebSocket:
                return std::unique_ptr<ITransport>(new WebSocketTransport());
            default:
                return nullptr;
        }
    }

    /**
     * @brief Create a transport by name
     * @param type Transport name ("stdio", "http", "websocket")
     * @return Unique pointer to transport, or nullptr if not found
     */
    static std::unique_ptr<ITransport> create(const std::string& type) {
        if (type == "stdio") return create(Type::Stdio);
        if (type == "http" || type == "sse") return create(Type::Http);
        if (type == "websocket" || type == "ws") return create(Type::WebSocket);
        return nullptr;
    }
};

} // namespace mcpp