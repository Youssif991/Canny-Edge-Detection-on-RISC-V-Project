#!/usr/bin/env bash
# =============================================================================
# step2_toolchain.sh — Build the RISC-V GCC toolchain with RVV support.
# Resumable: skips submodules already cloned, skips configure if already
# done, and skips the entire step if the compiler binary already exists.
# =============================================================================

step2_toolchain() {
    info "Step 2/3 — Building RISC-V GCC toolchain (this takes 30-45 min)..."

    # ── Skip entirely if already built ────────────────────────────────────────
    if [ -f "$RISCV_INSTALL/bin/riscv64-unknown-linux-gnu-gcc" ]; then
        warn "Toolchain already built at $RISCV_INSTALL — skipping entirely."
        export PATH="$RISCV_INSTALL/bin:$PATH"
        return
    fi

    mkdir -p "$RISCV_INSTALL"

    TOOLCHAIN_SRC="$HOME/riscv-gnu-toolchain"

    # ── Clone main repo only if not already cloned ────────────────────────────
    if [ ! -d "$TOOLCHAIN_SRC/.git" ]; then
        git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git "$TOOLCHAIN_SRC"
    else
        info "Toolchain repo already cloned — skipping."
    fi

    cd "$TOOLCHAIN_SRC"

    # ── Increase HTTP buffer to prevent mid-transfer drops ────────────────────
    git config --global http.postBuffer 524288000

    # ── Fetch each submodule individually, skipping already-present ones ──────
    for submodule in glibc binutils gdb gcc newlib; do
        if [ ! -f "$TOOLCHAIN_SRC/$submodule/.git" ] && \
           [ ! -d "$TOOLCHAIN_SRC/$submodule/.git" ]; then
            info "Fetching missing submodule: $submodule"
            git submodule update --init --depth 1 --progress -- "$submodule" || {
                warn "$submodule failed with --depth 1, retrying without depth limit..."
                git submodule update --init --progress -- "$submodule"
            }
        else
            info "Submodule already present: $submodule — skipping."
        fi
    done

    # ── Configure only if not already done ───────────────────────────────────
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
