#!/usr/bin/env bash
# =============================================================================
# main.sh — Phase 1 environment setup entry point
# Usage:
#   chmod +x phase1_setup/*.sh
#   ./phase1_setup/main.sh
#
# To run a single step in isolation:
#   source phase1_setup/utils.sh
#   source phase1_setup/step2_toolchain.sh
#   step2_toolchain
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── Load shared utilities and config ──────────────────────────────────────────
source "$SCRIPT_DIR/utils.sh"

# ── Load steps ────────────────────────────────────────────────────────────────
source "$SCRIPT_DIR/step1_prerequisites.sh"
source "$SCRIPT_DIR/step2_toolchain.sh"
source "$SCRIPT_DIR/step3_qemu.sh"

# ── Run ───────────────────────────────────────────────────────────────────────
echo ""
echo "============================================="
echo "  RISC-V RVV Phase 1 — Environment Setup"
echo "============================================="
echo ""

step1_prerequisites
step2_toolchain
step3_qemu

echo ""
success "Environment ready. Toolchain and QEMU are installed."
echo ""
echo "  Toolchain: $RISCV_INSTALL/bin/riscv64-unknown-linux-gnu-gcc"
echo "  QEMU:      $QEMU_INSTALL/bin/qemu-riscv64"
echo ""
echo "  Next: write your RVV test program and Makefile manually."
