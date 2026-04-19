#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
  cat <<EOF
Usage: $(basename "$0")

Install the Debian/Ubuntu packages required to build and test spio on a fresh
Linux container or VM.
EOF
}

log() {
  printf '[spio env] %s\n' "$*"
}

fail() {
  printf '[spio env] %s\n' "$*" >&2
  exit 1
}

as_root() {
  if [[ $EUID -eq 0 ]]; then
    "$@"
    return
  fi

  if command -v sudo >/dev/null 2>&1; then
    sudo "$@"
    return
  fi

  fail "sudo is required to install system packages"
}

ensure_debian_like() {
  if [[ ! -r /etc/os-release ]]; then
    fail "/etc/os-release is missing; only Debian/Ubuntu hosts are supported"
  fi

  # shellcheck disable=SC1091
  . /etc/os-release

  local family="${ID_LIKE:-}"
  if [[ "${ID:-}" != "debian" && "${ID:-}" != "ubuntu" && "${family}" != *debian* && "${family}" != *ubuntu* ]]; then
    fail "unsupported distribution: ${PRETTY_NAME:-unknown}. Expected Debian/Ubuntu."
  fi
}

install_system_packages() {
  local packages=(
    build-essential
    ca-certificates
    clang-18
    cmake
    curl
    git
    lld-18
    ninja-build
    pkg-config
    python3
    python3-pip
    python3-venv
  )

  log "installing system packages"
  as_root apt-get update
  as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${packages[@]}"
}

print_summary() {
  cat <<EOF

spio bootstrap complete.

Suggested shell exports:
  export CC=/usr/bin/clang-18
  export CXX=/usr/bin/clang++-18

Typical next steps:
  cmake -S "$ROOT" -B "$ROOT/build"
  cmake --build "$ROOT/build" -j"$(nproc)"
  ctest --test-dir "$ROOT/build"
EOF
}

main() {
  if [[ "${1:-}" == "--help" ]]; then
    usage
    exit 0
  fi

  ensure_debian_like
  install_system_packages
  print_summary
}

main "$@"
