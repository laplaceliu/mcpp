cmake_minimum_required(VERSION 3.10)

# 测试文件
set(TEST_SOURCES
    test_json.cpp
    test_message.cpp
    test_server.cpp
)

# 创建测试可执行文件
add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests
    mcpp
    GTest::gtest
    GTest::gmock
    pthread
)

# 添加测试
add_test(NAME mcpp_tests COMMAND tests)
