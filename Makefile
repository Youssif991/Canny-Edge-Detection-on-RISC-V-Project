# =============================================================================
# Makefile — Canny Edge Detection on RISC-V
# Author: Youssef
# =============================================================================

# -----------------------------------------------------------------------------
# Compilers and tools
# -----------------------------------------------------------------------------
HOST_CXX := g++
RV_CXX   := riscv64-unknown-elf-g++
QEMU     := qemu-riscv64

# -----------------------------------------------------------------------------
# Paths and flags
# -----------------------------------------------------------------------------
GTEST      := $(HOME)/googletest-installed
SRCS       := $(wildcard src/*.cpp)
LIB_SRCS   := $(filter-out src/main.cpp, $(SRCS))

RV_FLAGS   := -std=c++17 -march=rv64gcv -mabi=lp64d -O2 -static -Iinclude
HOST_FLAGS := -std=c++17 -O2 -Iinclude \
              -I$(GTEST)/include -L$(GTEST)/lib \
              -lgtest -lgtest_main -lpthread

VLEN_VALUES := 128 256 512

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
.PHONY: all clean run run_vlen test list-tests docs

all: canny_rv test

# Cross-compile the full pipeline for RISC-V
canny_rv: $(SRCS)
	@mkdir -p build/target/release
	$(RV_CXX) $(RV_FLAGS) $(SRCS) -o build/target/release/canny_rv.elf

# Build and run GoogleTest suite natively on host
test: tests/host_tests.cpp
	@mkdir -p build/host/debug
	$(HOST_CXX) -DHOST_MODE tests/host_tests.cpp $(LIB_SRCS) $(HOST_FLAGS) \
	    -o build/host/debug/unit_tests
	./build/host/debug/unit_tests

# Critical first test — verifies full toolchain and QEMU chain
rvv_test: tests/rvv_sanity.cpp
	@mkdir -p build/target/debug
	$(RV_CXX) $(RV_FLAGS) $< -o build/target/debug/rvv_sanity.elf
	@echo "=== VLEN=128 ===" && $(QEMU) -cpu rv64,v=true,vlen=128,elen=64 build/target/debug/rvv_sanity.elf
	@echo "=== VLEN=256 ===" && $(QEMU) -cpu rv64,v=true,vlen=256,elen=64 build/target/debug/rvv_sanity.elf
	@echo "=== VLEN=512 ===" && $(QEMU) -cpu rv64,v=true,vlen=512,elen=64 build/target/debug/rvv_sanity.elf

# Run the pipeline on QEMU at default VLEN
run: canny_rv
	$(QEMU) -cpu rv64,v=true,vlen=256,elen=64 build/target/release/canny_rv.elf

# Run at VLEN 128, 256, 512 to verify vector-length-agnostic correctness
run_vlen: canny_rv
	@for vlen in $(VLEN_VALUES); do \
	    echo "=== VLEN=$$vlen ==="; \
	    $(QEMU) -cpu rv64,v=true,vlen=$$vlen,elen=64 \
	        build/target/release/canny_rv.elf; \
	done

# Pattern rule — compile any test in tests/ to a RISC-V binary
build/target/debug/%.elf: tests/%.cpp $(LIB_SRCS)
	@mkdir -p build/target/debug
	$(RV_CXX) $(RV_FLAGS) $^ -o $@

# Run any test by name: make run-test NAME=sanity
run-test: build/target/debug/$(NAME).elf
	$(QEMU) -cpu rv64,v=true,vlen=256,elen=64 $<

# Generate Doxygen HTML documentation
docs:
	doxygen Doxyfile
	@echo "Docs generated — open docs/html/index.html to view."

# List all available tests
list-tests:
	@ls tests/*.cpp | xargs -n1 basename | sed 's/\.cpp//'

# Remove all build artifacts
clean:
	rm -rf build/
