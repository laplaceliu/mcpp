# MCPP - Modern C++ MCP Framework

A Modern C++ implementation of the Model Context Protocol (MCP), designed to be a balanced framework between simplicity and completeness.

## Features

- **Full MCP Protocol Support** (version 2025-06-18)
- **Multiple Transport Layers**: stdio and HTTP/SSE
- **Server Capabilities**: Tools, Resources, Prompts, Sampling, Elicitation, Logging
- **Client Capabilities**: Full client-side MCP support
- **Enterprise Features**:
  - Middleware/Filter Chain framework
  - Token Bucket Rate Limiting
  - Circuit Breaker pattern
  - Prometheus Metrics export
  - Bearer Token Authentication with RBAC

## Requirements

- C++14 compatible compiler (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.10+
- nlohmann/json 3.10.5
- cpp-httplib 0.15.3 (header-only)

## Installation

### Quick Start

```bash
# Clone the repository
git clone https://github.com/laplaceliu/mcpp.git
cd mcpp

# Set up dependencies (this downloads and builds all required libraries)
mkdir -p _deps && cd _deps
cmake CMakeLists.txt
make -j$(nproc)
cd ..

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Dependencies

The project uses CMake FetchContent to download dependencies. The dependency management is in `_deps/CMakeLists.txt`.

**Required system packages:**
- Ubuntu/Debian: `sudo apt-get install libssl-dev`
- macOS: OpenSSL is included (or `brew install openssl` if needed)
- Fedora/RHEL: `sudo dnf install openssl-devel`

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `MCPP_BUILD_TESTS` | ON | Build unit tests |
| `MCPP_BUILD_EXAMPLES` | ON | Build example applications |

## Project Structure

```
mcpp/
├── include/mcpp/
│   ├── core/           # Core types and utilities
│   │   ├── json.hpp    # JSON type aliases
│   │   ├── types.hpp   # MCP protocol types
│   │   └── error.hpp   # Error handling
│   ├── protocol/       # JSON-RPC protocol
│   │   ├── message.hpp # Message parsing/serialization
│   │   ├── request.hpp # Request routing
│   │   └── response.hpp
│   ├── transport/      # Transport layers
│   │   ├── transport.hpp
│   │   ├── stdio.hpp   # stdio transport
│   │   └── http.hpp    # HTTP/SSE transport
│   ├── server/         # Server implementation
│   │   ├── server.hpp
│   │   ├── tool.hpp
│   │   ├── resource.hpp
│   │   └── prompt.hpp
│   ├── client/         # Client implementation
│   │   └── client.hpp
│   └── enterprise/    # Enterprise features
│       ├── middleware.hpp
│       ├── ratelimit.hpp
│       ├── circuit.hpp
│       ├── metrics.hpp
│       └── auth.hpp
├── src/               # Source files
├── tests/             # Unit tests
└── examples/          # Example applications
```

## Usage

### Creating a Server

```cpp
#include "mcpp/server/server.hpp"
#include <iostream>

using namespace mcpp;

// Define a tool handler
CallToolResult echo_tool(const std::string& name, const JsonValue& args) {
    CallToolResult result;
    result.is_error = false;
    std::string input = args.value("message", std::string());
    result.content.push_back(JsonValue::object({
        {"type", "text"},
        {"text", "Echo: " + input}
    }));
    return result;
}

int main() {
    // Create server options
    ServerOptions options;
    options.name = "my-server";
    options.version = "1.0.0";
    options.capabilities.supports_tools = true;
    options.capabilities.supports_resources = true;

    // Create and start server
    Server server(options);

    // Register tools
    server.register_tool(
        "echo",
        "Echo back the input message",
        JsonValue::object({
            {"type", "object"},
            {"properties", JsonValue::object({
                {"message", JsonValue::object({
                    {"type", "string"},
                    {"description", "Message to echo"}
                })}
            })},
            {"required", JsonValue::array({"message"})}
        }),
        echo_tool
    );

    // Start server with stdio transport
    server.start();
    server.wait();

    return 0;
}
```

### Creating a Client

```cpp
#include "mcpp/client/client.hpp"
#include <iostream>

using namespace mcpp;

int main() {
    ClientOptions options;
    options.name = "my-client";
    options.version = "1.0.0";
    options.transport_type = TransportFactory::Type::Stdio;

    auto client = std::make_shared<Client>(options);

    // Handle sampling requests from server
    client->on_sampling_request([](const SamplingParams& params) {
        SamplingResult result;
        result.is_error = false;
        result.content = /* ... */;
        return result;
    });

    // Handle elicitation requests
    client->on_elicitation_request([](const ElicitationParams& params) {
        ElicitationResult result;
        result.is_error = false;
        result.content = /* ... */;
        return result;
    });

    client->start();
    client->wait();
    return 0;
}
```

### Using Enterprise Features

#### Rate Limiting

```cpp
#include "mcpp/enterprise/ratelimit.hpp"

using namespace mcpp::enterprise;

// Create rate limiter with custom config
RateLimiter::Config config;
config.tokens_per_second = 100;
config.max_tokens = 100;
config.tokens_per_request = 1;

RateLimiter limiter(config);

// Check if request is allowed
auto result = limiter.check("user_id");
if (result.allowed) {
    // Process request
} else {
    // Rate limited - retry_after_ms until more tokens
}
```

#### Circuit Breaker

```cpp
#include "mcpp/enterprise/circuit.hpp"

using namespace mcpp::enterprise;

CircuitBreaker breaker("my-service", CircuitBreaker::Config());

// Execute with circuit breaker protection
auto result = execute_with_circuit<int>(
    breaker,
    []() { return call_service(); },      // Primary function
    []() { return fallback_value(); }      // Fallback when circuit open
);
```

#### Prometheus Metrics

```cpp
#include "mcpp/enterprise/metrics.hpp"

using namespace mcpp::enterprise;

// Register metrics
auto& registry = MetricsRegistry::instance();
registry.counter("requests_total", "Total requests")->increment();
registry.gauge("active_connections", "Active connections")->set(42);
registry.histogram("request_duration", "Request duration")->observe(0.123);

// Export metrics in Prometheus format
std::string output = registry.export_prometheus();
```

#### Authentication & RBAC

```cpp
#include "mcpp/enterprise/auth.hpp"

using namespace mcpp::enterprise;

// Create authorizer with RBAC
Authorizer authorizer;

// Add roles and permissions
authorizer.add_permission("admin", "tools:*");
authorizer.add_permission("user", "tools:read");
authorizer.add_role("user1", "admin");

// Check permissions
if (authorizer.has_permission("user1", "tools:read")) {
    // Access granted
}

// Generate and validate tokens
Token token = authorizer.generate_token("user1", std::chrono::hours(24));
if (authorizer.validate_token(token)) {
    // Token valid
}
```

## Testing

```bash
# Run all tests
ctest --output-on-failure

# Or run tests directly
./build/bin/tests

# Run specific test suite
./build/bin/tests --gtest_filter="RateLimiterTest.*"
```

## API Reference

### Server

- `Server::Server(ServerOptions)` - Create server with options
- `Server::start()` - Start the server
- `Server::stop()` - Stop the server
- `Server::wait()` - Wait for shutdown
- `Server::register_tool(...)` - Register a tool
- `Server::register_resource(...)` - Register a resource
- `Server::register_prompt(...)` - Register a prompt

### Client

- `Client::Client(ClientOptions)` - Create client with options
- `Client::start()` - Start the client
- `Client::stop()` - Stop the client
- `Client::on_sampling_request(...)` - Set sampling handler
- `Client::on_elicitation_request(...)` - Set elicitation handler
- `Client::on_progress_notification(...)` - Set progress handler
- `Client::logging_message(...)` - Send logging message

### Enterprise

- `RateLimiter::check(key)` - Check rate limit for key
- `CircuitBreaker::allow_request()` - Check if request allowed
- `CircuitBreaker::record_success()` - Record successful call
- `CircuitBreaker::record_failure()` - Record failed call
- `MetricsRegistry::counter(name, help)` - Get/create counter
- `MetricsRegistry::gauge(name, help)` - Get/create gauge
- `MetricsRegistry::histogram(name, help)` - Get/create histogram
- `Authorizer::has_permission(role, permission)` - Check permission
- `Authorizer::generate_token(user_id, duration)` - Generate token

## Documentation

### Building API Documentation

This project uses Doxygen for API documentation. To generate the documentation:

```bash
# Install Doxygen (if not already installed)
# Ubuntu/Debian:
sudo apt-get install doxygen

# macOS:
brew install doxygen

# Fedora/RHEL:
sudo dnf install doxygen

# Generate documentation (run from project root)
doxygen docs/Doxyfile.in

# View the documentation
# Open docs/doxygen/html/index.html in a browser
open docs/doxygen/html/index.html  # macOS
xdg-open docs/doxygen/html/index.html  # Linux
```

The generated documentation will be in `docs/doxygen/html/` directory:
- `index.html` - Main documentation page
- `classes.html` - Class hierarchy
- `files.html` - File list

## License

MIT License - see LICENSE file for details
