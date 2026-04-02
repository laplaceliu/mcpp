#!/bin/bash
# MCP Inspector Test Script for stdio_server
# This script runs automated tests against the MCP stdio server

# Don't exit on error, we want to run all tests
# set -e

# Relative path from examples directory to build output
SERVER_PATH="../build/bin/example_stdio_server"
TEST_RESULTS=""
PASSED=0
FAILED=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to send a request and check response
send_request() {
    local test_name="$1"
    local request="$2"
    local expected_contains="$3"
    
    echo -e "${YELLOW}Testing: $test_name${NC}"
    
    # Send request and capture response
    response=$(echo "$request" | timeout 3 "$SERVER_PATH" 2>/dev/null || true)
    
    if echo "$response" | grep -q "$expected_contains"; then
        echo -e "${GREEN}✓ PASSED${NC}"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}✗ FAILED${NC}"
        echo "  Request: $request"
        echo "  Expected to contain: $expected_contains"
        echo "  Got: $response"
        ((FAILED++))
        return 1
    fi
}

echo "=========================================="
echo "MCP Inspector Test Suite"
echo "=========================================="
echo ""

# Build server if needed
if [ ! -f "$SERVER_PATH" ]; then
    echo "Building server..."
    # Go to project root (parent of examples)
    cd ..
    mkdir -p build
    cd build
    cmake .. -DMCPP_BUILD_EXAMPLES=ON -DMCPP_BUILD_TESTS=OFF
    make example_stdio_server
    cd ../examples
fi

echo "Starting tests..."
echo ""

# Test Group 1: Basic Connection
echo "=== Test Group 1: Basic Connection ==="
send_request "Initialize" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}' \
    "comprehensive-mcp-server"

send_request "Initialize Capabilities" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}' \
    "listChanged"

echo ""

# Test Group 2: Tools
echo "=== Test Group 2: Tools ==="

# Initialize first, then list tools
response=$(echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}' | timeout 3 "$SERVER_PATH" 2>/dev/null)

send_request "Tools List" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' \
    "echo"

send_request "Echo Tool" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"echo","arguments":{"message":"hello"}}}' \
    "hello"

send_request "Echo Uppercase" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"echo","arguments":{"message":"hello","format":"uppercase"}}}' \
    "HELLO"

send_request "Calculator Add" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"calculator","arguments":{"a":10,"b":5,"operation":"add"}}}' \
    "15"

send_request "Calculator Division by Zero" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"calculator","arguments":{"a":10,"b":0,"operation":"divide"}}}' \
    "error"

send_request "Random Tool" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"random","arguments":{"min":1,"max":10,"count":3}}}' \
    "Random"

send_request "Time Tool" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"time","arguments":{"format":"unix"}}}' \
    "20"

send_request "Analyze Tool" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"analyze","arguments":{"text":"hello world","include_stats":true}}}' \
    "Characters"

send_request "JSON Validate" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"json","arguments":{"operation":"validate","data":"{}"}}}' \
    "Valid"

echo ""

# Test Group 3: Resources
echo "=== Test Group 3: Resources ==="
send_request "Resources List" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"resources/list","params":{}}' \
    "config://server"

send_request "Read Config Resource" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"resources/read","params":{"uri":"config://server"}}' \
    "server_name"

send_request "Read System Resource" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"resources/read","params":{"uri":"system://info"}}' \
    "os"

echo ""

# Test Group 4: Prompts
echo "=== Test Group 4: Prompts ==="
send_request "Prompts List" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"prompts/list","params":{}}' \
    "code_review"

send_request "Get Code Review Prompt" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"prompts/get","params":{"name":"code_review","arguments":{"code":"int main() {}","language":"cpp"}}}' \
    "review"

send_request "Get Generate Docs Prompt" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"prompts/get","params":{"name":"generate_docs","arguments":{"type":"code","content":"test"}}}' \
    "documentation"

echo ""

# Test Group 5: Error Handling
echo "=== Test Group 5: Error Handling ==="
send_request "Invalid Method" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"invalid_method","params":{}}' \
    "Method not found"

send_request "Missing Tool Name" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"arguments":{}}}' \
    "Missing"

echo ""

# Summary
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed! ✓${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed! ✗${NC}"
    exit 1
fi
