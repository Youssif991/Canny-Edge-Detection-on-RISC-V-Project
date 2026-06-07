#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# step2_toolchain.sh
# Build the RISC-V GCC toolchain with RVV (V extension) support.
# Produces: riscv64-unknown-elf-g++ (bare-metal Newlib toolchain)
# Resumable: skips submodules already cloned, skips configure if already
# done, and skips entirely if the compiler binary already exists.
# Full clones only — shallow clones omit files needed by the build system.
# -----------------------------------------------------------------------------

step2_toolchain() {
    info "Step 2/5 — Building RISC-V GCC toolchain (this takes 30-90 min)..."

    # Skip entirely if already built
    if [ -f "$RISCV_INSTALL/bin/riscv64-unknown-elf-g++" ]; then
        warn "Toolchain already built at $RISCV_INSTALL — skipping entirely."
        export PATH="$RISCV_INSTALL/bin:$PATH"
        return
    fi

    mkdir -p "$RISCV_INSTALL"

    TOOLCHAIN_SRC="$HOME/riscv-gnu-toolchain"

    # Clone main repo only if not already cloned
    if [ ! -d "$TOOLCHAIN_SRC/.git" ]; then
        git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git "$TOOLCHAIN_SRC"
    else
        info "Toolchain repo already cloned — skipping."
    fi

    cd "$TOOLCHAIN_SRC"

    # Fetch each submodule individually, skipping already-present ones
    for submodule in glibc binutils gdb gcc newlib; do
        if [ ! -f "$TOOLCHAIN_SRC/$submodule/.git" ] && \
           [ ! -d "$TOOLCHAIN_SRC/$submodule/.git" ]; then
            info "Fetching missing submodule: $submodule (full clone)..."
            git -c http.postBuffer=524288000 submodule update --init --progress -- "$submodule"
        else
            info "Submodule already present: $submodule — skipping."
        fi
    done

    # Configure only if not already done
    if [ ! -f "$TOOLCHAIN_SRC/Makefile" ] || \
       ! grep -q "rv64gcv" "$TOOLCHAIN_SRC/Makefile" 2>/dev/null; then
        ./configure \
            --prefix="$RISCV_INSTALL" \
            --with-arch=rv64gcv \
            --with-abi=lp64d
    else
        info "Already configured — skipping configure step."
    fi

    # Build the bare-metal Newlib toolchain (produces riscv64-unknown-elf-g++)
    make -j"$JOBS"

    export PATH="$RISCV_INSTALL/bin:$PATH"

    # Verify compiler exists and reports correct version
    need riscv64-unknown-elf-g++
    info "Compiler version: $(riscv64-unknown-elf-g++ --version | head -1)"

    # Verify it produces a valid RISC-V ELF binary
    echo 'int main(){return 0;}' > /tmp/rvv_verify.c
    riscv64-unknown-elf-g++ -march=rv64gcv -mabi=lp64d -o /tmp/rvv_verify /tmp/rvv_verify.c
    local file_out
    file_out=$(file /tmp/rvv_verify)
    if echo "$file_out" | grep -q "RISC-V"; then
        success "RISC-V GCC toolchain verified: produces valid RISC-V ELF binary."
    else
        error "Toolchain verification failed — binary is not RISC-V ELF: $file_out"
    fi
    rm -f /tmp/rvv_verify.c /tmp/rvv_verify
}
