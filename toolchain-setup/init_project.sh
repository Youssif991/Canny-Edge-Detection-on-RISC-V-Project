#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# init_project.sh
# Creates the project folder structure with placeholder files.
# Run once from the project root after cloning the repo.
#
# Usage:
#   chmod +x init_project.sh
#   ./init_project.sh
# -----------------------------------------------------------------------------

set -euo pipefail

# Colours
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

info "Creating project structure at $PROJECT_ROOT..."

# -----------------------------------------------------------------------------
# Directories
# -----------------------------------------------------------------------------
mkdir -p "$PROJECT_ROOT"/src
mkdir -p "$PROJECT_ROOT"/include
mkdir -p "$PROJECT_ROOT"/tests
mkdir -p "$PROJECT_ROOT"/assets
mkdir -p "$PROJECT_ROOT"/tools
mkdir -p "$PROJECT_ROOT"/docs
mkdir -p "$PROJECT_ROOT"/build/host/debug
mkdir -p "$PROJECT_ROOT"/build/target/release
mkdir -p "$PROJECT_ROOT"/build/target/debug

# -----------------------------------------------------------------------------
# Placeholder source files (so git tracks the empty folders)
# -----------------------------------------------------------------------------

# Main pipeline entry point
if [ ! -f "$PROJECT_ROOT/src/main.cpp" ]; then
    cat > "$PROJECT_ROOT/src/main.cpp" << 'EOF'
/**
 * @file main.cpp
 * @brief Entry point for the Canny Edge Detection pipeline.
 * @author Youssef
 */

#include <cstdio>

int main(int argc, char* argv[]) {
    // TODO: parse width, height, input/output paths from argv
    // TODO: load raw grayscale image
    // TODO: run pipeline stages
    // TODO: save output image
    printf("Canny Edge Detection — RISC-V RVV\n");
    return 0;
}
EOF
    info "Created src/main.cpp"
fi

# Gaussian blur stage
if [ ! -f "$PROJECT_ROOT/src/gaussian.cpp" ]; then
    cat > "$PROJECT_ROOT/src/gaussian.cpp" << 'EOF'
/**
 * @file gaussian.cpp
 * @brief 5x5 Gaussian blur — scalar baseline implementation.
 * @author Youssef
 */

#include "gaussian.hpp"

// TODO: implement gaussian_blur()
EOF
    info "Created src/gaussian.cpp"
fi

# Sobel gradient stage
if [ ! -f "$PROJECT_ROOT/src/sobel.cpp" ]; then
    cat > "$PROJECT_ROOT/src/sobel.cpp" << 'EOF'
/**
 * @file sobel.cpp
 * @brief Sobel gradient computation (Gx, Gy) — scalar baseline.
 * @author Youssef
 */

#include "sobel.hpp"

// TODO: implement sobel_gradient()
EOF
    info "Created src/sobel.cpp"
fi

# Magnitude stage
if [ ! -f "$PROJECT_ROOT/src/magnitude.cpp" ]; then
    cat > "$PROJECT_ROOT/src/magnitude.cpp" << 'EOF'
/**
 * @file magnitude.cpp
 * @brief Gradient magnitude — L1 and L2 norm implementations.
 * @author Youssef
 */

#include "magnitude.hpp"

// TODO: implement magnitude_l1() and magnitude_l2()
EOF
    info "Created src/magnitude.cpp"
fi

# Direction stage
if [ ! -f "$PROJECT_ROOT/src/direction.cpp" ]; then
    cat > "$PROJECT_ROOT/src/direction.cpp" << 'EOF'
/**
 * @file direction.cpp
 * @brief Gradient direction quantized to 0, 45, 90, 135 degrees.
 * @author Youssef
 */

#include "direction.hpp"

// TODO: implement gradient_direction()
EOF
    info "Created src/direction.cpp"
fi

# Image I/O
if [ ! -f "$PROJECT_ROOT/src/image_io.cpp" ]; then
    cat > "$PROJECT_ROOT/src/image_io.cpp" << 'EOF'
/**
 * @file image_io.cpp
 * @brief Raw grayscale image load and save (width * height bytes, no header).
 * @author Youssef
 */

#include "image_io.hpp"

// TODO: implement load_image() and save_image()
EOF
    info "Created src/image_io.cpp"
fi

# -----------------------------------------------------------------------------
# Header placeholders
# -----------------------------------------------------------------------------
for header in gaussian sobel magnitude direction image_io; do
    if [ ! -f "$PROJECT_ROOT/include/${header}.hpp" ]; then
        cat > "$PROJECT_ROOT/include/${header}.hpp" << EOF
/**
 * @file ${header}.hpp
 * @brief Interface for the ${header} pipeline stage.
 * @author Youssef
 */

#pragma once

#include <cstdint>

// TODO: declare functions for ${header} stage
EOF
        info "Created include/${header}.hpp"
    fi
done

# -----------------------------------------------------------------------------
# Host test placeholder
# -----------------------------------------------------------------------------
if [ ! -f "$PROJECT_ROOT/tests/host_tests.cpp" ]; then
    cat > "$PROJECT_ROOT/tests/host_tests.cpp" << 'EOF'
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
EOF
    info "Created tests/host_tests.cpp"
fi

# -----------------------------------------------------------------------------
# keep git from ignoring empty build dirs
# -----------------------------------------------------------------------------
touch "$PROJECT_ROOT/build/host/debug/.gitkeep"
touch "$PROJECT_ROOT/build/target/release/.gitkeep"
touch "$PROJECT_ROOT/build/target/debug/.gitkeep"
touch "$PROJECT_ROOT/assets/.gitkeep"
touch "$PROJECT_ROOT/docs/.gitkeep"
touch "$PROJECT_ROOT/tools/.gitkeep"

echo ""
success "Project structure ready."
echo ""
echo "  src/          — pipeline stage source files"
echo "  include/      — header files"
echo "  tests/        — GoogleTest host-side tests"
echo "  assets/       — raw grayscale test images"
echo "  tools/        — helper scripts (image generator, viewer)"
echo "  docs/         — Doxygen generated output"
echo "  build/        — compiled binaries (not committed)"
echo ""
echo "  Next: run 'make all' once you have implemented the pipeline."
