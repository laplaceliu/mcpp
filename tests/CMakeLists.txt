cmake_minimum_required(VERSION 3.10)

# Test files
set(TEST_SOURCES
    main.cpp
    test_json.cpp
    test_message.cpp
    test_server.cpp
    test_ratelimit.cpp
    test_circuit.cpp
    test_metrics.cpp
    test_transport.cpp
)

# Create test executable
add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests
    mcpp
    GTest::gtest
    GTest::gmock
    pthread
)

# Add tests
add_test(NAME mcpp_tests COMMAND tests)
