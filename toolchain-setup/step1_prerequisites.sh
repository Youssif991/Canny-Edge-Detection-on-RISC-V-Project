#!/usr/bin/env bash

# step1_prerequisites.sh — Install host packages required to build the
# RISC-V toolchain and QEMU from source.

step1_prerequisites() {
    info "Step 1/3 — Installing host prerequisites..."

    sudo apt-get update -qq
    sudo apt-get install -y \
        git curl wget build-essential cmake ninja-build \
        libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev \
        libslirp-dev libssl-dev python3 python3-pip \
        flex bison autoconf automake libtool pkg-config \
        libgmp-dev libmpfr-dev libmpc-dev texinfo

    need git
    need cmake
    need ninja
    need gcc

    success "Host prerequisites installed."
}
