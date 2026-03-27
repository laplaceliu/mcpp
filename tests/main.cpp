/**
 * @file main.cpp
 * @brief Main entry point for GoogleTest test suite
 */
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
