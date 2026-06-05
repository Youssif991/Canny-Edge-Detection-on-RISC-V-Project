#!/usr/bin/env bash
# =============================================================================
# Phase 1 Setup Script — RISC-V RVV Toolchain + QEMU + GoogleTest
# =============================================================================
# Usage:
#   chmod +x phase1_setup.sh
#   ./phase1_setup.sh
#
# What this script does:
#   1. Installs host prerequisites via apt
#   2. Builds the RISC-V GCC toolchain with RVV (V extension) support
#   3. Builds QEMU from source (riscv64 user-mode)
#   4. Creates the project directory structure
#   5. Writes the RVV test program, Makefile, and a host GoogleTest file
#   6. Clones GoogleTest as a submodule
#   7. Compiles and runs the RVV test at VLEN 128, 256, and 512
# =============================================================================

set -euo pipefail   # exit on error, undefined var, or pipe failure

# ── Colour helpers ────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # no colour

info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

# ── Configuration — edit these if needed ─────────────────────────────────────
RISCV_INSTALL="$HOME/riscv"
QEMU_INSTALL="$HOME/qemu-install"
QEMU_VERSION="v8.2.0"
PROJECT_DIR="$HOME/rvv-project"
JOBS=$(nproc)

# ── Helper: check a command exists ───────────────────────────────────────────
need() {
    command -v "$1" &>/dev/null || error "Required command '$1' not found after install — something went wrong."
}

# =============================================================================
# STEP 1 — Host prerequisites
# =============================================================================
step1_prerequisites() {
    info "Step 1/7 — Installing host prerequisites..."

    sudo apt-get update -qq
    sudo apt-get install -y \
        git curl wget build-essential cmake ninja-build \
        libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev \
        libslirp-dev libssl-dev python3 python3-pip \
        flex bison autoconf automake libtool pkg-config \
        libgmp-dev libmpfr-dev libmpc-dev texinfo

    need git; need cmake; need ninja; need gcc
    success "Host prerequisites installed."
}

# =============================================================================
# STEP 2 — Build RISC-V GCC toolchain with RVV support
# =============================================================================
# =============================================================================
# STEP 2 — Build RISC-V GCC toolchain with RVV support
# =============================================================================
step2_toolchain() {
    info "Step 2/7 — Building RISC-V GCC toolchain (this takes 30-45 min)..."

    if [ -f "$RISCV_INSTALL/bin/riscv64-unknown-linux-gnu-gcc" ]; then
        warn "Toolchain already built at $RISCV_INSTALL — skipping entirely."
        export PATH="$RISCV_INSTALL/bin:$PATH"
        return
    fi

    mkdir -p "$RISCV_INSTALL"

    TOOLCHAIN_SRC="$HOME/riscv-gnu-toolchain"

    # ── Clone main repo only if not already cloned ───────────────────────────
    if [ ! -d "$TOOLCHAIN_SRC/.git" ]; then
        git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git "$TOOLCHAIN_SRC"
    else
        info "Toolchain repo already cloned — skipping."
    fi

    cd "$TOOLCHAIN_SRC"

    # ── Increase buffer to avoid mid-transfer drops ───────────────────────────
    git config --global http.postBuffer 524288000

    # ── Init submodules if not already done ───────────────────────────────────
    for submodule in glibc binutils gdb gcc newlib; do
        if [ ! -f "$TOOLCHAIN_SRC/$submodule/.git" ] && \
           [ ! -d "$TOOLCHAIN_SRC/$submodule/.git" ]; then
            info "Fetching missing submodule: $submodule"
            git submodule update --init --depth 1 --progress -- "$submodule" || {
                warn "$submodule failed with depth 1, retrying without depth limit..."
                git submodule update --init --progress -- "$submodule"
            }
        else
            info "Submodule already present: $submodule — skipping."
        fi
    done

    # ── Configure only if not already configured ──────────────────────────────
    if [ ! -f "$TOOLCHAIN_SRC/Makefile" ] || \
       ! grep -q "rv64gcv" "$TOOLCHAIN_SRC/Makefile" 2>/dev/null; then
        ./configure \
            --prefix="$RISCV_INSTALL" \
            --with-arch=rv64gcv \
            --with-abi=lp64d \
            --with-multilib-generator="rv64gcv-lp64d--"
    else
        info "Already configured — skipping configure step."
    fi

    # ── Build ─────────────────────────────────────────────────────────────────
    make linux -j"$JOBS"

    export PATH="$RISCV_INSTALL/bin:$PATH"
    _add_to_path "$RISCV_INSTALL/bin"

    need riscv64-unknown-linux-gnu-gcc
    success "RISC-V GCC toolchain built: $(riscv64-unknown-linux-gnu-gcc --version | head -1)"
}

# =============================================================================
# STEP 3 — Build QEMU from source (riscv64 user-mode)
# =============================================================================
step3_qemu() {
    info "Step 3/7 — Building QEMU $QEMU_VERSION (riscv64 user-mode)..."

    if [ -f "$QEMU_INSTALL/bin/qemu-riscv64" ]; then
        warn "QEMU already exists at $QEMU_INSTALL — skipping build."
        export PATH="$QEMU_INSTALL/bin:$PATH"
        return
    fi

    QEMU_SRC="$HOME/qemu"
    if [ ! -d "$QEMU_SRC" ]; then
        git clone https://gitlab.com/qemu-project/qemu.git "$QEMU_SRC"
    fi

    cd "$QEMU_SRC"
    git checkout "$QEMU_VERSION"

    mkdir -p build && cd build

    ../configure \
        --target-list=riscv64-linux-user \
        --prefix="$QEMU_INSTALL" \
        --disable-docs \
        --disable-system

    make -j"$JOBS"
    make install

    export PATH="$QEMU_INSTALL/bin:$PATH"
    _add_to_path "$QEMU_INSTALL/bin"

    need qemu-riscv64
    success "QEMU built: $(qemu-riscv64 --version | head -1)"
}

# =============================================================================
# STEP 4 — Create project structure
# =============================================================================
step4_project_structure() {
    info "Step 4/7 — Creating project structure at $PROJECT_DIR..."

    mkdir -p "$PROJECT_DIR"/{src,tests,host_tests,third_party,build}
    cd "$PROJECT_DIR"

    # Init git repo if not already
    if [ ! -d ".git" ]; then
    git init
    git remote add origin https://github.com/Youssif991/Canny-Edge-Detection-on-RISC-V-Project.git
    info "Git repo initialised and linked to remote."
elif ! git remote get-url origin &>/dev/null; then
    git remote add origin https://github.com/Youssif991/Canny-Edge-Detection-on-RISC-V-Project.git
    info "Remote origin added to existing repo."
else
    info "Remote origin already set: $(git remote get-url origin)"
fi

    # .gitignore
    cat > .gitignore << 'EOF'
build/
*.o
*.a
*.elf
third_party/googletest/build/
EOF

    success "Project structure created."
}

# =============================================================================
# STEP 5 — Write source files (test program + Makefile + host test)
# =============================================================================
step5_write_sources() {
    info "Step 5/7 — Writing source files..."
    cd "$PROJECT_DIR"

    # ── RVV test program ──────────────────────────────────────────────────────
    cat > tests/rvv_add_test.c << 'EOF'
#include <stdio.h>
#include <stdint.h>
#include <riscv_vector.h>

int main(void) {
    const int N = 16;
    int32_t a[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    int32_t b[16] = {16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
    int32_t c[16] = {0};

    size_t n = N;
    int32_t *pa = a, *pb = b, *pc = c;

    while (n > 0) {
        size_t vl = __riscv_vsetvl_e32m1(n);
        vint32m1_t va = __riscv_vle32_v_i32m1(pa, vl);
        vint32m1_t vb = __riscv_vle32_v_i32m1(pb, vl);
        vint32m1_t vc = __riscv_vadd_vv_i32m1(va, vb, vl);
        __riscv_vse32_v_i32m1(pc, vc, vl);
        pa += vl; pb += vl; pc += vl; n -= vl;
    }

    int ok = 1;
    for (int i = 0; i < N; i++) {
        if (c[i] != 17) {
            printf("FAIL at index %d: got %d, expected 17\n", i, c[i]);
            ok = 0;
        }
    }
    printf("%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
EOF

    # ── Host GoogleTest placeholder ───────────────────────────────────────────
    cat > host_tests/host_test.cpp << 'EOF'
#include <gtest/gtest.h>

// Scalar reference implementation — runs on your host CPU, no RISC-V needed.
static void vector_add(const int* a, const int* b, int* c, int n) {
    for (int i = 0; i < n; i++) c[i] = a[i] + b[i];
}

TEST(VectorAdd, AllSeventeen) {
    int a[4] = {1, 5, 9,  3};
    int b[4] = {16, 12, 8, 14};
    int c[4] = {0};
    vector_add(a, b, c, 4);
    for (int i = 0; i < 4; i++) EXPECT_EQ(c[i], 17);
}

TEST(VectorAdd, LengthOne) {
    int a[1] = {7}, b[1] = {10}, c[1] = {0};
    vector_add(a, b, c, 1);
    EXPECT_EQ(c[0], 17);
}
EOF

    # ── Makefile ──────────────────────────────────────────────────────────────
    cat > Makefile << 'MAKEFILE_EOF'
# ── Toolchain ────────────────────────────────────────────────────────────────
RISCV_PREFIX ?= riscv64-unknown-linux-gnu-
CC_RISCV     := $(RISCV_PREFIX)gcc
QEMU         := qemu-riscv64
SYSROOT      := $(HOME)/riscv/sysroot

# ── RISC-V build flags ───────────────────────────────────────────────────────
ARCH_FLAGS   := -march=rv64gcv -mabi=lp64d
OPT_FLAGS    := -O2
RISCV_CFLAGS := $(ARCH_FLAGS) $(OPT_FLAGS)

# ── Host (native) flags ──────────────────────────────────────────────────────
CC_HOST      := g++
HOST_CFLAGS  := -O2 -Wall -std=c++17

# ── Directories ──────────────────────────────────────────────────────────────
BUILD_DIR    := build
VLEN_VALUES  := 128 256 512

.PHONY: all riscv host test-riscv test-host test-all clean

all: riscv host

# ── RISC-V targets ───────────────────────────────────────────────────────────
riscv: $(addprefix $(BUILD_DIR)/rvv_add_test_vlen, $(VLEN_VALUES))

$(BUILD_DIR)/rvv_add_test_vlen%: tests/rvv_add_test.c | $(BUILD_DIR)
	$(CC_RISCV) $(RISCV_CFLAGS) -o $@ $<

# ── Run on QEMU at each VLEN ─────────────────────────────────────────────────
test-riscv: riscv
	@for vlen in $(VLEN_VALUES); do \
	    echo "=== VLEN=$$vlen ==="; \
	    $(QEMU) -L $(SYSROOT) \
	            -cpu rv64,v=true,vlen=$$vlen,elen=64 \
	            $(BUILD_DIR)/rvv_add_test_vlen$$vlen; \
	done

# ── GoogleTest ───────────────────────────────────────────────────────────────
GTEST_DIR  := third_party/googletest
GTEST_INC  := -I$(GTEST_DIR)/googletest/include
GTEST_LIBS := $(BUILD_DIR)/libgtest.a $(BUILD_DIR)/libgtest_main.a

$(GTEST_LIBS): | $(BUILD_DIR)
	cmake -S $(GTEST_DIR) -B $(GTEST_DIR)/build \
	    -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build $(GTEST_DIR)/build
	cp $(GTEST_DIR)/build/lib/libgtest*.a $(BUILD_DIR)/

host: $(BUILD_DIR)/host_tests

$(BUILD_DIR)/host_tests: host_tests/host_test.cpp $(GTEST_LIBS) | $(BUILD_DIR)
	$(CC_HOST) $(HOST_CFLAGS) $(GTEST_INC) \
	    -o $@ $< -L$(BUILD_DIR) -lgtest_main -lgtest -lpthread

test-host: $(BUILD_DIR)/host_tests
	./$(BUILD_DIR)/host_tests

test-all: test-riscv test-host

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
MAKEFILE_EOF

    success "Source files written."
}

# =============================================================================
# STEP 6 — Clone GoogleTest
# =============================================================================
step6_googletest() {
    info "Step 6/7 — Setting up GoogleTest..."
    cd "$PROJECT_DIR"

    if [ ! -d "third_party/googletest/.git" ]; then
        git submodule add https://github.com/google/googletest.git third_party/googletest
        git submodule update --init
    else
        warn "GoogleTest already present — skipping clone."
    fi

    success "GoogleTest ready."
}

# =============================================================================
# STEP 7 — Build and verify
# =============================================================================
step7_verify() {
    info "Step 7/7 — Building and verifying the full chain..."
    cd "$PROJECT_DIR"

    export PATH="$RISCV_INSTALL/bin:$QEMU_INSTALL/bin:$PATH"

    make all

    echo ""
    info "Running RISC-V RVV tests at VLEN 128, 256, 512..."
    make test-riscv

    echo ""
    info "Running host GoogleTest..."
    make test-host

    echo ""
    success "Phase 1 complete! All tests passed."
    echo ""
    echo "  Project:   $PROJECT_DIR"
    echo "  Toolchain: $RISCV_INSTALL/bin/riscv64-unknown-linux-gnu-gcc"
    echo "  QEMU:      $QEMU_INSTALL/bin/qemu-riscv64"
    echo ""
    echo "  Next: cd $PROJECT_DIR && make test-all"
}

# =============================================================================
# Utility: add a path to ~/.bashrc (idempotent)
# =============================================================================
_add_to_path() {
    local dir="$1"
    local marker="export PATH=\"$dir:\$PATH\""
    if ! grep -qF "$marker" "$HOME/.bashrc" 2>/dev/null; then
        echo "$marker" >> "$HOME/.bashrc"
    fi
}

# =============================================================================
# Main
# =============================================================================
main() {
    echo ""
    echo "============================================="
    echo "  RISC-V RVV Phase 1 — Automated Setup"
    echo "============================================="
    echo ""

    step1_prerequisites
    step2_toolchain
    step3_qemu
    step4_project_structure
    step5_write_sources
    step6_googletest
    step7_verify
}

main "$@"