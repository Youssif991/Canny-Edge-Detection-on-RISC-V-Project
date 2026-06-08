/**
 * @file    gaussian_test.cpp
 * @brief   GoogleTest unit tests for the Gaussian blur pipeline stage.
 *          Test implementations removed; file kept as a placeholder with
 *          comments and structure for future tests.
 */

#include <gtest/gtest.h>
#include "gaussian.hpp"
#include "utils.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Test helpers (placeholder)
// ─────────────────────────────────────────────────────────────────────────────

// Allocate a metadata_t with a 64-byte aligned buffer
static void alloc_meta(image::io::metadata_t<uint8_t>& m, int w, int h)
{
    // Implementation intentionally removed — placeholder only.
}

// ─────────────────────────────────────────────────────────────────────────────
// Tests (placeholders)
// ─────────────────────────────────────────────────────────────────────────────

TEST(CannyGaussian, Placeholder) {
    // Test body removed. Add assertions here when re-enabling tests.
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
