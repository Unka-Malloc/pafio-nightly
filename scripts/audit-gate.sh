#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

RANGE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --range)
      RANGE="$2"
      shift 2
      ;;
    -h|--help)
      echo 'Usage: scripts/audit-gate.sh [--range <rev-range>]'
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -n "$RANGE" ]]; then
  exec python3 "$ROOT/scripts/secret-scan.py" --range "$RANGE"
fi
exec python3 "$ROOT/scripts/secret-scan.py"
