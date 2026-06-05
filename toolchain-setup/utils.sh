#!/usr/bin/env bash
# =============================================================================
# utils.sh — Shared helpers and configuration
# Sourced by main.sh and all step files
# =============================================================================

# ── Colours ───────────────────────────────────────────────────────────────────
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

# ── Shared configuration ──────────────────────────────────────────────────────
RISCV_INSTALL="$HOME/riscv"
QEMU_INSTALL="$HOME/qemu-install"
QEMU_VERSION="v8.2.0"
JOBS=$(nproc)

# ── Add a directory to PATH in ~/.bashrc (idempotent) ─────────────────────────
_add_to_path() {
    local dir="$1"
    local marker="export PATH=\"$dir:\$PATH\""
    if ! grep -qF "$marker" "$HOME/.bashrc" 2>/dev/null; then
        echo "$marker" >> "$HOME/.bashrc"
    fi
}
