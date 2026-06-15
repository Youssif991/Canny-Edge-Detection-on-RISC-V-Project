#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# step1_prerequisites.sh
# Install host packages required to build the RISC-V toolchain and QEMU.
# Supports Ubuntu/Debian and Arch/Manjaro.
# -----------------------------------------------------------------------------

step1_prerequisites() {
    info "Step 1/5 — Installing host prerequisites..."

    local os
    os=$(_detect_os)

    if [ "$os" = "ubuntu" ] || [ "$os" = "debian" ] || [ "$os" = "pop" ]; then
        sudo apt-get update -qq
        sudo apt-get install -y \
            autoconf automake build-essential bison flex texinfo \
            gperf libtool patchutils bc git cmake ninja-build \
            libglib2.0-dev libpixman-1-dev libslirp-dev libfdt-dev \
            libmpc-dev libmpfr-dev libgmp-dev zlib1g-dev \
            libexpat1-dev libssl-dev python3 python3-venv python3-pip \
            doxygen doxygen-gui doxygen-latex doxygen-doc graphviz \
            pkg-config curl wget libpulse0 libgtk-3-0t64 libasound2t64 \
            libdbus-1-3 libxkbcommon-x11-0 libxcb-icccm4 libxcb-image0 \
            libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 \
            libxcb-xinput0 libxcb-xfixes0 libqt5gui5t64 \
            gcc-riscv64-linux-gnu g++-riscv64-linux-gnu \
            binutils-riscv64-linux-gnu libc6-dev-riscv64-cross

    elif [ "$os" = "arch" ] || [ "$os" = "manjaro" ]; then
        sudo pacman -Syu --needed --noconfirm \
            base-devel multilib-devel git cmake ninja \
            glib2 pixman libslirp gmp mpc mpfr expat zlib \
            python doxygen graphviz

    else
        error "Unsupported OS: $os. Supported: ubuntu, debian, pop, arch, manjaro."
    fi

    need git; need cmake; need ninja; need gcc; need python3

    success "Host prerequisites installed."
}
