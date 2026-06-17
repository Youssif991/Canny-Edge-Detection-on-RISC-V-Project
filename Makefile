# =============================================================================
# Makefile — Canny Edge Detection on RISC-V
# Author: Youssef
# =============================================================================

# -----------------------------------------------------------------------------
# Compilers and tools
# -----------------------------------------------------------------------------
HOST_CXX := g++
RV_CXX   := riscv64-linux-gnu-g++
QEMU     := qemu-riscv64

# -----------------------------------------------------------------------------
# Paths and flags
# -----------------------------------------------------------------------------
GTEST      := $(HOME)/googletest-installed
SRCS       := $(wildcard src/*.cpp)
LIB_SRCS   := $(filter-out src/main.cpp src/bench.cpp, $(SRCS))
TEST_SRCS  := $(wildcard tests/*.cpp)

RV_FLAGS   := -std=c++23 -march=rv64gcv -mabi=lp64d -O2 -static -Iinclude
HOST_FLAGS := -std=c++23 -O2 -Iinclude \
              -I$(GTEST)/include -L$(GTEST)/lib \
              -lgtest -lgtest_main -lpthread

VLEN_VALUES := 128 256 512

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
.PHONY: all clean run run_vlen test test-file test-all rvv_test list-tests docs

all: canny_rv

# Cross-compile the full pipeline for RISC-V
canny_rv: $(SRCS)
	@mkdir -p build/target/release
	$(RV_CXX) $(RV_FLAGS) $(SRCS) -o build/target/release/canny_rv.elf

# -----------------------------------------------------------------------------
# Phase 4 Benchmarks
# -----------------------------------------------------------------------------

bench_O0:
	@mkdir -p build/target/release
	$(RV_CXX) -std=gnu++23 -march=rv64gcv -mabi=lp64d -O0 -static -Iinclude $(SRCS) -o build/target/release/bench_O0.elf
	@echo "Built bench_O0"

bench_O2:
	@mkdir -p build/target/release
	$(RV_CXX) -std=gnu++23 -march=rv64gcv -mabi=lp64d -O2 -static -Iinclude $(SRCS) -o build/target/release/bench_O2.elf
	@echo "Built bench_O2"

bench_O3:
	@mkdir -p build/target/release
	$(RV_CXX) -std=gnu++23 -march=rv64gcv -mabi=lp64d -O3 -static -Iinclude $(SRCS) -o build/target/release/bench_O3.elf
	@echo "Built bench_O3"

bench_O3vec:
	@mkdir -p build/target/release
	$(RV_CXX) -std=gnu++23 -march=rv64gcv -mabi=lp64d -O3 -ftree-vectorize -fopt-info-vec-all -static -Iinclude $(SRCS) -o build/target/release/bench_O3vec.elf 2> build/target/release/vec_report.txt
	@echo "Built bench_O3vec. Vectorization report saved to build/target/release/vec_report.txt"

bench_sweep: bench_O0 bench_O2 bench_O3 bench_O3vec

run_bench: bench_sweep
	@echo "\n========================================================"
	@echo "=== Step 3: Binary Sizes                             ==="
	@echo "========================================================"
	riscv64-unknown-elf-size build/target/release/bench_O0.elf
	riscv64-unknown-elf-size build/target/release/bench_O2.elf
	riscv64-unknown-elf-size build/target/release/bench_O3.elf
	riscv64-unknown-elf-size build/target/release/bench_O3vec.elf
	@echo "\n========================================================"
	@echo "=== Step 6: QEMU Execution                           ==="
	@echo "========================================================"
	@echo "\n--- Running -O0 ---"
	$(QEMU) -cpu rv64,v=true,vlen=128 build/target/release/bench_O0.elf
	@echo "\n--- Running -O2 ---"
	$(QEMU) -cpu rv64,v=true,vlen=128 build/target/release/bench_O2.elf
	@echo "\n--- Running -O3 ---"
	$(QEMU) -cpu rv64,v=true,vlen=128 build/target/release/bench_O3.elf
	@echo "\n--- Running -O3 with Auto-vectorization ---"
	$(QEMU) -cpu rv64,v=true,vlen=128 build/target/release/bench_O3vec.elf

# -----------------------------------------------------------------------------
# Host tests
# -----------------------------------------------------------------------------

# Run all test files at once
test-all:
	@mkdir -p build/host/debug
	$(HOST_CXX) -DHOST_MODE $(TEST_SRCS) $(LIB_SRCS) $(HOST_FLAGS) \
	    -o build/host/debug/unit_tests
	./build/host/debug/unit_tests

# Run a specific test file: make test FILE=test_gaussian
# Example: make test FILE=host_tests
#          make test FILE=test_gaussian
test:
ifndef FILE
	$(error Usage: make test FILE=<test_name_without_extension>)
endif
	@mkdir -p build/host/debug
	$(HOST_CXX) -DHOST_MODE tests/$(FILE).cpp $(LIB_SRCS) $(HOST_FLAGS) \
	    -o build/host/debug/$(FILE)
	./build/host/debug/$(FILE)

# Run a specific test filtered by name: make test-filter FILE=test_gaussian FILTER=Gaussian2D
test-filter:
ifndef FILE
	$(error Usage: make test-filter FILE=<test_name> FILTER=<test_suite>)
endif
	@mkdir -p build/host/debug
	$(HOST_CXX) -DHOST_MODE tests/$(FILE).cpp $(LIB_SRCS) $(HOST_FLAGS) \
	    -o build/host/debug/$(FILE)
	./build/host/debug/$(FILE) --gtest_filter=$(FILTER)*

# -----------------------------------------------------------------------------
# RISC-V tests
# -----------------------------------------------------------------------------

# Command pattern: make rvv_test FILE=your_test_filename
rvv_test:
	@mkdir -p build/target/debug
	@if [ ! -f "tests/$(FILE).cpp" ]; then \
		echo "Error: tests/$(FILE).cpp not found!"; exit 1; \
	fi
	
	@echo "Compiling tests/$(FILE).cpp with library dependencies..."
	$(RV_CXX) $(RV_FLAGS) tests/$(FILE).cpp $(LIB_SRCS) -o build/target/debug/$(FILE).elf
	
	@echo "\n=== Running under QEMU simulations ==="
	@echo "=== VLEN=128 ===" && $(QEMU) -cpu rv64,v=true,vlen=128,elen=64 build/target/debug/$(FILE).elf
	@echo "=== VLEN=256 ===" && $(QEMU) -cpu rv64,v=true,vlen=256,elen=64 build/target/debug/$(FILE).elf
	@echo "=== VLEN=512 ===" && $(QEMU) -cpu rv64,v=true,vlen=512,elen=64 build/target/debug/$(FILE).elf

# Run a specific QEMU test at a chosen VLEN: make rvv_test_vlen FILE=gaussian_test VLEN=256
rvv_test_vlen:
ifndef FILE
	$(error Usage: make rvv_test_vlen FILE=<test_name> VLEN=<128|256|512>)
endif
ifndef VLEN
	$(error Usage: make rvv_test_vlen FILE=<test_name> VLEN=<128|256|512>)
endif
	@mkdir -p build/target/debug
	$(RV_CXX) $(RV_FLAGS) tests/$(FILE).cpp $(LIB_SRCS) -o build/target/debug/$(FILE).elf
	@echo "=== Running under QEMU with VLEN=$(VLEN) ==="
	$(QEMU) -cpu rv64,v=true,vlen=$(VLEN),elen=64 build/target/debug/$(FILE).elf
# Run the pipeline on QEMU at default VLEN
run: canny_rv
	$(QEMU) -cpu rv64,v=true,vlen=256,elen=64 build/target/release/canny_rv.elf

# Run at VLEN 128, 256, 512 to verify VLA correctness
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

# Run any RISC-V test by name: make run-test NAME=rvv_sanity
run-test: build/target/debug/$(NAME).elf
	$(QEMU) -cpu rv64,v=true,vlen=256,elen=64 build/target/debug/$(NAME).elf

# -----------------------------------------------------------------------------
# Utilities
# -----------------------------------------------------------------------------

# Generate Doxygen documentation
docs:
	python3 docs/m.css/documentation/doxygen.py docs/conf.py
	@echo "Docs generated — open docs/html/index.html to view."

# List all available host test files
list-tests:
	@echo "Available test files:"
	@ls tests/*.cpp | xargs -n1 basename | sed 's/\.cpp//'

#Generate Images
gen-images:
	python3 tools/gen_test_image.py

# View a single raw image file: make view FILE=output
view:
ifndef FILE
	$(error Usage: make view FILE=<path_to_raw_file_without_extension>)
endif
	python3 tools/view_raw.py $(FILE).raw
 
# View all raw assets in ./assets/ and save PNGs alongside each .raw file
view_all:
	@echo "Rendering all raw files in assets/..."
	@for f in assets/*.raw; do \
		echo "  [view] $$f"; \
		python3 tools/view_raw.py $$f; \
	done
	@echo "Done. PNGs saved next to each .raw file in assets/."
		

# Remove all build artifacts
clean:
	rm -rf build/


# =============================================================================
# Phase 4 — Compiler Optimization Sweep
# =============================================================================
# Usage:
#   make sweep          — builds and runs -O0 / -O2 / -O3 in sequence
#   make bench OPT=-O0  — build + run one level manually
#   make bench-autovec  — build + run with auto-vectorization report
#
# After each run, note the printed timings and binary size into your table.
# =============================================================================

BENCH_SRC  := tests/bench_pipeline.cpp
BENCH_BASE := -std=c++23 -march=rv64gcv -mabi=lp64d -static -Iinclude
QEMU_CPU   := -cpu rv64,v=true,vlen=256,elen=64

# Build + run a single optimization level: make bench OPT=-O2
bench:
ifndef OPT
	$(error Usage: make bench OPT=<-O0|-O2|-O3>)
endif
	@mkdir -p build/target/bench
	$(RV_CXX) $(BENCH_BASE) $(OPT) $(BENCH_SRC) $(LIB_SRCS) \
	    -o build/target/bench/bench_$(subst -,,$(OPT)).elf
	@echo "--- Binary size ---"
	@size build/target/bench/bench_$(subst -,,$(OPT)).elf
	@echo "--- Running on QEMU ---"
	$(QEMU) $(QEMU_CPU) build/target/bench/bench_$(subst -,,$(OPT)).elf

# Build all three levels then run them one by one (fills the whole table)
sweep:
	@mkdir -p build/target/bench
	@for opt in O0 O2 O3; do \
		echo ""; \
		echo "========================================="; \
		echo "======== Compiling for Optimization -$$opt ========"; \
		echo "========================================="; \
		$(RV_CXX) $(BENCH_BASE) -$$opt $(BENCH_SRC) $(LIB_SRCS) \
			-o build/target/bench/bench_$$opt.elf; \
		echo "--- Binary size ---"; \
		size build/target/bench/bench_$$opt.elf; \
		echo -n "Total Binary Size: "; \
		size build/target/bench/bench_$$opt.elf | tail -n 1 | awk '{print $$4 " Bytes (" $$4/(1024*1024) " MB)"}'; \
		for vlen in 128 256 512; do \
			echo ""; \
			echo ">>> Running with Configuration: -$$opt | VLEN=$$vlen"; \
			echo "-------------------------------------------------"; \
			$(QEMU) -cpu rv64,v=true,vlen=$$vlen,elen=64 build/target/bench/bench_$$opt.elf; \
		done; \
	done
