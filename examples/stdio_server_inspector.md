# MCP Inspector Testing Guide for stdio_server

## Installation

```bash
# Option 1: Using npx (recommended, no global install needed)
npx @modelcontextprotocol/inspector

# Option 2: Global installation
npm install -g @modelcontextprotocol/inspector
mcp-inspector
```

## Launch Inspector

### Method 1: Command Line
```bash
# Navigate to project root
cd <project-root>

# Build the server
cd build && cmake .. -DMCPP_BUILD_EXAMPLES=ON && make example_stdio_server

# Launch Inspector
npx @modelcontextprotocol/inspector ./build/bin/example_stdio_server
```

### Method 2: Configuration File
Create `mcp-inspector-config.json`:
```json
{
  "servers": {
    "mcpp_stdio": {
      "command": "./build/bin/example_stdio_server",
      "args": [],
      "env": {}
    }
  }
}
```

Then run:
```bash
npx @modelcontextprotocol/inspector --config mcp-inspector-config.json
```

## Test Checklist

### Test Group 1: Basic Connection

| # | Test Item | Action | Expected Result |
|---|-----------|--------|-----------------|
| 1.1 | Server Startup | Select server in Inspector | Status shows "Connected" |
| 1.2 | Initialize Request | Send `initialize` | Returns serverInfo and capabilities |
| 1.3 | Protocol Version | Check protocolVersion | Should be "2025-06-18" |
| 1.4 | Server Name | Check serverInfo.name | Should be "comprehensive-mcp-server" |
| 1.5 | Capabilities | Check capabilities | Should include tools/resources/prompts/logging |

**Inspector Test Command:**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2025-06-18",
    "capabilities": {}
  }
}
```

### Test Group 2: Tools List

| # | Test Item | Action | Expected Result |
|---|-----------|--------|-----------------|
| 2.1 | List Tools | Send `tools/list` | Returns 7 tools |
| 2.2 | Echo Tool | Check tool definition | Includes message and format parameters |
| 2.3 | Calculator Tool | Check tool definition | Includes a, b, operation parameters |
| 2.4 | Random Tool | Check tool definition | Includes min, max, count parameters |
| 2.5 | Time Tool | Check tool definition | Includes format parameter |
| 2.6 | Analyze Tool | Check tool definition | Includes text parameter |
| 2.7 | JSON Tool | Check tool definition | Includes operation and data parameters |
| 2.8 | Generate Image Tool | Check tool definition | Includes prompt and size parameters |

**Inspector Test Command:**
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/list",
  "params": {}
}
```

### Test Group 3: Tools Call

#### 3.1 Echo Tool Tests

| # | Scenario | Request Params | Expected Response |
|---|----------|----------------|-------------------|
| 3.1.1 | Basic Echo | `{"name":"echo","arguments":{"message":"hello"}}` | Contains "hello" |
| 3.1.2 | Uppercase | `{"name":"echo","arguments":{"message":"hello","format":"uppercase"}}` | Contains "HELLO" |
| 3.1.3 | Lowercase | `{"name":"echo","arguments":{"message":"HELLO","format":"lowercase"}}` | Contains "hello" |
| 3.1.4 | Reverse | `{"name":"echo","arguments":{"message":"hello","format":"reverse"}}` | Contains "olleh" |

#### 3.2 Calculator Tool Tests

| # | Scenario | Request Params | Expected Response |
|---|----------|----------------|-------------------|
| 3.2.1 | Addition | `{"a":10,"b":5,"operation":"add"}` | Contains "15" |
| 3.2.2 | Subtraction | `{"a":10,"b":5,"operation":"subtract"}` | Contains "5" |
| 3.2.3 | Multiplication | `{"a":10,"b":5,"operation":"multiply"}` | Contains "50" |
| 3.2.4 | Division | `{"a":10,"b":5,"operation":"divide"}` | Contains "2" |
| 3.2.5 | Division by Zero | `{"a":10,"b":0,"operation":"divide"}` | Returns error |
| 3.2.6 | Unknown Op | `{"a":10,"b":5,"operation":"unknown"}` | Returns error |

#### 3.3 Random Tool Tests

| # | Scenario | Request Params | Expected Response |
|---|----------|----------------|-------------------|
| 3.3.1 | Default | `{}` | Returns 1 number (0-100) |
| 3.3.2 | Custom Range | `{"min":1,"max":10}` | Returns number in range |
| 3.3.3 | Multiple | `{"count":5}` | Returns 5 numbers |

#### 3.4 Time Tool Tests

| # | Scenario | Request Params | Expected Response |
|---|----------|----------------|-------------------|
| 3.4.1 | ISO8601 | `{"format":"iso8601"}` | Returns timestamp string |
| 3.4.2 | Unix | `{"format":"unix"}` | Returns numeric timestamp |
| 3.4.3 | Date | `{"format":"date"}` | Returns YYYY-MM-DD |
| 3.4.4 | Time | `{"format":"time"}` | Returns HH:MM:SS |

#### 3.5 Analyze Tool Tests

| # | Scenario | Request Params | Expected Response |
|---|----------|----------------|-------------------|
| 3.5.1 | Basic Stats | `{"text":"hello world","include_stats":true}` | Contains char/word counts |
| 3.5.2 | Word Freq | `{"text":"hello hello world","include_words":true}` | Contains frequency stats |

#### 3.6 JSON Tool Tests

| # | Scenario | Request Params | Expected Response |
|---|----------|----------------|-------------------|
| 3.6.1 | Validate Valid | `{"operation":"validate","data":"{}"}` | Returns "Valid JSON" |
| 3.6.2 | Validate Invalid | `{"operation":"validate","data":"invalid"}` | Returns error |
| 3.6.3 | Prettify | `{"operation":"prettify","data":"{\"a\":1}"}` | Returns formatted JSON |
| 3.6.4 | Minify | `{"operation":"minify","data":"{\"a\":1}"}` | Returns single-line JSON |
| 3.6.5 | Keys | `{"operation":"keys","data":"{\"a\":1,\"b\":2}"}` | Returns key list |

**Inspector Test Command Template:**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "echo",
    "arguments": {
      "message": "hello"
    }
  }
}
```

### Test Group 4: Resources

#### 4.1 Resources List

| # | Test Item | Action | Expected Result |
|---|-----------|--------|-----------------|
| 4.1.1 | List Resources | `resources/list` | Returns 2 resources + 1 template |
| 4.1.2 | Check config | - | Contains config://server |
| 4.1.3 | Check system | - | Contains system://info |
| 4.1.4 | Check template | - | Contains user://{user_id}/profile |

**Inspector Test Command:**
```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "method": "resources/list",
  "params": {}
}
```

#### 4.2 Resources Read

| # | Test Item | URI | Expected Result |
|---|-----------|-----|-----------------|
| 4.2.1 | Read Config | `config://server` | Returns server config JSON |
| 4.2.2 | Read System | `system://info` | Returns system info |
| 4.2.3 | Invalid Resource | `invalid://test` | Returns error |

**Inspector Test Command:**
```json
{
  "jsonrpc": "2.0",
  "id": 5,
  "method": "resources/read",
  "params": {
    "uri": "config://server"
  }
}
```

### Test Group 5: Prompts

#### 5.1 Prompts List

| # | Test Item | Action | Expected Result |
|---|-----------|--------|-----------------|
| 5.1.1 | List Prompts | `prompts/list` | Returns 3 prompts |
| 5.1.2 | Code Review | - | Contains code_review |
| 5.1.3 | Generate Docs | - | Contains generate_docs |
| 5.1.4 | Generate Tests | - | Contains generate_tests |

**Inspector Test Command:**
```json
{
  "jsonrpc": "2.0",
  "id": 6,
  "method": "prompts/list",
  "params": {}
}
```

#### 5.2 Prompts Get

| # | Test Item | Params | Expected Result |
|---|-----------|--------|-----------------|
| 5.2.1 | Code Review | `{"name":"code_review","arguments":{"code":"test"}}` | Returns review messages |
| 5.2.2 | Generate Docs | `{"name":"generate_docs","arguments":{"type":"code"}}` | Returns doc messages |
| 5.2.3 | Generate Tests | `{"name":"generate_tests","arguments":{"code":"test"}}` | Returns test messages |
| 5.2.4 | Invalid Prompt | `{"name":"invalid"}` | Returns error |

**Inspector Test Command:**
```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "prompts/get",
  "params": {
    "name": "code_review",
    "arguments": {
      "code": "int main() { return 0; }",
      "language": "cpp"
    }
  }
}
```

### Test Group 6: Error Handling

| # | Scenario | Request | Error Code | Error Message |
|---|----------|---------|------------|---------------|
| 6.1 | Invalid Method | `{"method":"invalid"}` | -32601 | Method not found |
| 6.2 | Invalid Params | `{"method":"tools/call","params":{}}` | -32602 | Missing tool name |
| 6.3 | Parse Error | Send invalid JSON | -32700 | Parse error |
| 6.4 | Internal Error | Call unconfigured feature | -32603 | Internal error |

### Test Group 7: Notifications

| # | Test Item | Method | Notes |
|---|-----------|--------|-------|
| 7.1 | Tools Changed | `notifications/tools/list_changed` | Server supports |
| 7.2 | Resources Changed | `notifications/resources/list_changed` | Server supports |
| 7.3 | Resource Updated | `notifications/resources/updated` | Server supports |
| 7.4 | Log Message | `notifications/message` | Server supports |

## Using Inspector UI

### Step 1: Launch Inspector
```bash
# Navigate to project root directory
cd <project-root>
npx @modelcontextprotocol/inspector ./build/bin/example_stdio_server
```

### Step 2: Open in Browser
Inspector will launch a Web UI, typically at `http://localhost:5173`

### Step 3: Execute Tests
1. **Connect Server**: Click "Connect" button
2. **View Server Info**: Check initialize response
3. **Browse Tools**: Click "Tools" tab to view all tools
4. **Test Tool Call**: Select a tool, fill parameters, click "Call"
5. **Browse Resources**: Click "Resources" tab
6. **Read Resource**: Click resource URI to view content
7. **Browse Prompts**: Click "Prompts" tab
8. **Get Prompt**: Select prompt template, fill parameters, click "Get"

## Expected Test Results

### Pass Criteria
- All tool calls return correct results
- Resource reads return valid content
- Prompt gets return correct message structure
- Error handling returns standard error codes
- No stderr output to stdout

### Troubleshooting

| Issue | Possible Cause | Solution |
|-------|---------------|----------|
| Connection failed | Server not started or wrong path | Check executable path |
| Parse error | Invalid JSON format | Check request JSON format |
| Method not found | Method not registered | Check server implementation |
| No response | Server crashed | Check stderr logs |

## Automated Testing Script

Use the provided test script:
```bash
# Navigate to examples directory
cd examples
./stdio_server_inspector.sh
```

## Test Report Template

After testing, fill in:

```markdown
## MCP Inspector Test Report

### Test Environment
- Date: 
- Inspector Version: 
- Server Version: 1.0.0

### Test Results
| Test Group | Passed | Failed | Notes |
|------------|--------|--------|-------|
| Basic Connection |  |  |  |
| Tools List |  |  |  |
| Tools Call |  |  |  |
| Resources |  |  |  |
| Prompts |  |  |  |
| Error Handling |  |  |  |

### Issues Found
1. 
2. 

### Conclusion
- [ ] All tests passed
- [ ] Some tests failed
- [ ] Need fixes and retest
```

## Quick Test Reference

Complete functional test:
```bash
cd build

cat << 'EOF' | ./bin/example_stdio_server
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{}}}
{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"echo","arguments":{"message":"hello"}}}
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"calculator","arguments":{"a":10,"b":5,"operation":"add"}}}
{"jsonrpc":"2.0","id":5,"method":"resources/list","params":{}}
{"jsonrpc":"2.0","id":6,"method":"resources/read","params":{"uri":"config://server"}}
{"jsonrpc":"2.0","id":7,"method":"prompts/list","params":{}}
{"jsonrpc":"2.0","id":8,"method":"prompts/get","params":{"name":"code_review","arguments":{"code":"test"}}}
EOF
```

---

**Test Executor**: Automated Testing  
**Last Updated**: 2025-04-02
