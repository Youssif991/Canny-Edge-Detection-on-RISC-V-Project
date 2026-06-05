#!/usr/bin/env bash
# =============================================================================
# step3_qemu.sh — Build QEMU from source targeting riscv64 user-mode.
# Skips the build if qemu-riscv64 already exists at the install prefix.
# =============================================================================

step3_qemu() {
    info "Step 3/3 — Building QEMU $QEMU_VERSION (riscv64 user-mode)..."

    # ── Skip if already built ─────────────────────────────────────────────────
    if [ -f "$QEMU_INSTALL/bin/qemu-riscv64" ]; then
        warn "QEMU already exists at $QEMU_INSTALL — skipping build."
        export PATH="$QEMU_INSTALL/bin:$PATH"
        return
    fi

    QEMU_SRC="$HOME/qemu"

    # ── Clone if not already present ──────────────────────────────────────────
    if [ ! -d "$QEMU_SRC/.git" ]; then
        git clone https://gitlab.com/qemu-project/qemu.git "$QEMU_SRC"
    else
        info "QEMU source already cloned — skipping."
    fi

    cd "$QEMU_SRC"
    git checkout "$QEMU_VERSION"

    # ── Configure and build ───────────────────────────────────────────────────
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
