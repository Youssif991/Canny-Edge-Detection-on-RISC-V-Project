#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# utils.sh
# Shared helpers, configuration, and PATH setup
# Sourced by main.sh and all step files
# -----------------------------------------------------------------------------

# Colours
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()    { echo -e "${CYAN}[INFO]${NC}  $*"; }
success() { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }
need()    { command -v "$1" &>/dev/null || error "Required command '$1' not found after install."; }

# Shared configuration
RISCV_INSTALL="$HOME/riscv-toolchain"
QEMU_INSTALL="$HOME/qemu-install"
QEMU_VERSION="v8.2.0"
GTEST_INSTALL="$HOME/googletest-installed"
PROJECT_TITLE="RVV-Canny-Edge-Detection"
VENV_DIR=".venv"
PYTHON_PACKAGES="numpy matplotlib PyQt5 jinja2 Pygments"
JOBS=$(nproc)

# OS detection
_detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    else
        error "Unable to detect OS from /etc/os-release"
    fi
}

# Print ASCII header
_print_header() {
    echo -e "${CYAN}"
    echo " ██████╗ ██╗   ██╗██╗   ██╗"
    echo " ██╔══██╗██║   ██║██║   ██║"
    echo " ██████╔╝██║   ██║██║   ██║"
    echo " ██╔══██╗╚██╗ ██╔╝╚██╗ ██╔╝"
    echo " ██║  ██║ ╚████╔╝  ╚████╔╝ "
    echo " ╚═╝  ╚═╝  ╚═══╝    ╚═══╝  "
    echo ""
    echo "   RISC-V VECTOR TOOLCHAIN SETUP"
    echo "   Canny Edge Detection Project"
    echo "   Author: Youssef"
    echo -e "${NC}"
    sleep 1
}

# Ensure toolchain and QEMU are always on PATH
_setup_paths() {
    export PATH="$HOME/riscv-toolchain/bin:$HOME/qemu-install/bin:$PATH"

    if ! grep -qF 'riscv-toolchain/bin' "$HOME/.bashrc" 2>/dev/null; then
        echo '' >> "$HOME/.bashrc"
        echo '# RISC-V toolchain and QEMU — added by toolchain-setup' >> "$HOME/.bashrc"
        echo 'export PATH="$HOME/riscv-toolchain/bin:$HOME/qemu-install/bin:$PATH"' >> "$HOME/.bashrc"
    fi
}
