#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# main.sh
# RISC-V RVV Environment Setup — Phase 1
# Canny Edge Detection Project
# Author: Youssef
#
# Usage:
#   chmod +x toolchain-setup/*.sh
#   ./toolchain-setup/main.sh
#
# To run a single step in isolation:
#   source toolchain-setup/utils.sh
#   source toolchain-setup/step2_toolchain.sh
#   step2_toolchain
# -----------------------------------------------------------------------------

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

source "$SCRIPT_DIR/utils.sh"
source "$SCRIPT_DIR/step1_prerequisites.sh"
source "$SCRIPT_DIR/step2_toolchain.sh"
source "$SCRIPT_DIR/step3_qemu.sh"
source "$SCRIPT_DIR/step4_project.sh"
source "$SCRIPT_DIR/step5_python.sh"

_print_header

step1_prerequisites
step2_toolchain
step3_qemu
step4_project_structure
step5_python
_setup_paths

echo ""
success "Environment ready. All tools installed."
echo ""
echo "  Toolchain : $RISCV_INSTALL/bin/riscv64-unknown-elf-g++"
echo "  QEMU      : $QEMU_INSTALL/bin/qemu-riscv64"
echo "  Project   : $HOME/$PROJECT_TITLE"
echo "  Python    : $HOME/$PROJECT_TITLE/$VENV_DIR"
echo ""
echo "  Run 'source ~/.bashrc' to reload PATH in your current shell."
echo "  Run 'source $VENV_DIR/bin/activate' inside the project to use Python."
