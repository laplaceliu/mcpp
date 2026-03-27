/**
 * @file transport.cpp
 * @brief Transport factory implementation
 */
#include "mcpp/transport/transport.hpp"
#include "mcpp/transport/stdio.hpp"
#include "mcpp/transport/http.hpp"
#include "mcpp/transport/websocket.hpp"
#include <memory>

namespace mcpp {

std::unique_ptr<ITransport> TransportFactory::create(Type type) {
    switch (type) {
        case Type::Stdio:
            return std::unique_ptr<ITransport>(new StdioTransport());
        case Type::Http:
            // Default HTTP transport on port 8080
            return std::unique_ptr<ITransport>(new HttpTransport(8080));
        case Type::WebSocket:
            return std::unique_ptr<ITransport>(new WebSocketTransport());
        default:
            return nullptr;
    }
}

std::unique_ptr<ITransport> TransportFactory::create(const std::string& type) {
    if (type == "stdio") return create(Type::Stdio);
    if (type == "http" || type == "sse") return create(Type::Http);
    if (type == "websocket" || type == "ws") return create(Type::WebSocket);
    return nullptr;
}

} // namespace mcpp
