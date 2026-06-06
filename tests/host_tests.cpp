/**
 * @file host_tests.cpp
 * @brief GoogleTest suite — host-side unit tests for each pipeline stage.
 *        Compiled natively with g++, no RISC-V required.
 * @author Youssef
 */

#include <gtest/gtest.h>

// TODO: include pipeline headers
// TODO: add tests per stage (Phase 3)

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
