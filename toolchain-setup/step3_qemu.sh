#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# step3_qemu.sh
# Build QEMU from source targeting riscv64 user-mode.
# Built with --enable-plugins for profiling support (required by hints guide).
# Skips the build if qemu-riscv64 already exists at the install prefix.
# -----------------------------------------------------------------------------

step3_qemu() {
    info "Step 3/5 — Building QEMU (riscv64 user-mode)..."

    if [ -f "$QEMU_INSTALL/bin/qemu-riscv64" ]; then
        warn "QEMU already exists at $QEMU_INSTALL — skipping build."
        export PATH="$QEMU_INSTALL/bin:$PATH"
        return
    fi

    QEMU_SRC="$HOME/qemu"

    if [ ! -d "$QEMU_SRC/.git" ]; then
        git clone https://github.com/qemu/qemu.git "$QEMU_SRC"
    else
        info "QEMU source already cloned — skipping."
    fi

    cd "$QEMU_SRC"

    mkdir -p build && cd build

    ../configure \
        --target-list=riscv64-linux-user \
        --prefix="$QEMU_INSTALL" \
        --enable-plugins \
        --disable-docs \
        --disable-system

    make -j"$JOBS"
    make install

    export PATH="$QEMU_INSTALL/bin:$PATH"

    need qemu-riscv64

    # Verify version is 9.x or newer as required by the hints guide
    local version
    version=$(qemu-riscv64 --version | head -1)
    local major
    major=$(echo "$version" | grep -o '[0-9]\+' | head -1)
    if [ "$major" -ge 9 ]; then
        success "QEMU verified: $version"
    else
        warn "QEMU version is $version — hints guide recommends 9.x or newer."
    fi
}
