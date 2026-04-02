#!/bin/bash
# Test script for stdio_client and stdio_server pair

CLIENT="../build/bin/example_stdio_client"
SERVER="../build/bin/example_stdio_server"

# Check if binaries exist
if [ ! -f "$CLIENT" ]; then
    echo "Client not found: $CLIENT"
    exit 1
fi

if [ ! -f "$SERVER" ]; then
    echo "Server not found: $SERVER"
    exit 1
fi

echo "Testing MCP stdio client-server pair..."
echo ""

# Test 1: Direct server test
echo "=== Test 1: Direct server response test ==="
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}' | timeout 3 "$SERVER" 2>/dev/null
echo ""

# Test 2: Client request format verification
echo "=== Test 2: Client request format ==="
echo "Client sends these requests:"
echo '{"id":1,"jsonrpc":"2.0","method":"initialize","params":{"capabilities":{},"protocolVersion":"2025-06-18"}}'
echo '{"id":2,"jsonrpc":"2.0","method":"tools/list","params":{}}'
echo ""

# Test 3: Manual interaction test
echo "=== Test 3: Manual test commands ==="
echo "To test manually, run these in separate terminals:"
echo "Terminal 1: $SERVER"
echo "Terminal 2: $CLIENT"
echo ""
echo "Or use this command to test the pair:"
echo "$SERVER | $CLIENT"
echo ""

echo "Note: The client and server communicate via stdin/stdout."
echo "For proper testing, use MCP Inspector:"
echo "npx @modelcontextprotocol/inspector $SERVER"
