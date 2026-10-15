#!/usr/bin/env bash
# Registered engineering entrypoints. Defining a command does not execute it.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"
case "${1:-}" in
  baseline)
    cmake -S . -B build-codex -DPAFIO_BUILD_TESTS=ON
    cmake --build build-codex
    build-codex/bin/pafio --help
    ;;
  focused)
    cmake --build build-codex --target pafio pafio_native_tests
    build-codex/bin/pafio_native_tests --gtest_filter='BuildPlanTests.*:BuildCliTests.*:RunCliTests.*:TestCliTests.*:CheckCliTests.*:SyncCliTests.*'
    python3 scripts/styio-interface-gate.py --styio-bin "${STYIO_BIN:-$PWD/../styio-nightly/build/default/bin/styio}" --pafio-bin "$PWD/build-codex/bin/pafio" --require-compile-plan --task-resource --work-dir "$PWD/build-codex/task-resource-focused" --json
    ;;
  integration)
    cmake --build build-codex --target pafio
    python3 scripts/styio-interface-gate.py --styio-bin "${STYIO_BIN:-$PWD/../styio-nightly/build/default/bin/styio}" --pafio-bin "$PWD/build-codex/bin/pafio" --require-compile-plan --task-resource --work-dir "$PWD/build-codex/task-resource-integration" --json
    ;;
  final)
    # Runs the complete submit gate once under the retained-evidence wrapper;
    # each host invocation keeps its own host-* record. Any diagnosed-failure
    # recovery is a per-run selection, never a registered entrypoint.
    python3 docs/plans/task-resource-delivery/retain-final.py \
      python3 scripts/submit-gate.py --profile ci --json
    ;;
  review)
    git diff --check
    ;;
  *) echo "usage: checks.sh {baseline|focused|integration|final|review}" >&2; exit 2 ;;
esac
