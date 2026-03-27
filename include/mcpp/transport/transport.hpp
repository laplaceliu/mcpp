/**
 * @file transport.hpp
 * @brief Abstract transport layer interface and factory
 *
 * This header is provided for backwards compatibility.
 * For new code, consider including:
 *   - itransport.hpp   : ITransport interface only
 *   - factory.hpp      : ITransport + TransportFactory + all transports
 *   - stdio.hpp        : Stdio transport only
 *   - http.hpp         : HTTP transport only
 *   - websocket.hpp    : WebSocket transport only
 */
#pragma once

#include "factory.hpp"