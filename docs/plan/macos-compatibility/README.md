# macOS Compatibility Plan

**Purpose:** Manage macOS-specific compatibility work for `spio`, including native CI smoke, CLI installer behavior, managed Styio installation, release artifact policy, and real-machine validation evidence.

**Last updated:** 2026-07-28

## State Files

- `../Manifest.json` indexes this plan.
- `Checkpoints.json` is the executable checkpoint graph for macOS compatibility.
- Shared evidence format lives in [../spio-foundation/Evidence-Template.md](../spio-foundation/Evidence-Template.md).

## Target Matrix

1. `darwin-aarch64` for Apple Silicon macOS.

## Current Evidence Snapshot

1. `local-ci-gate` includes a `macos-latest` native CMake build and native test smoke.
2. Release target docs map `darwin-*` to `styio-macos-cli`.
3. The Unix installer recognizes `darwin-aarch64`, creates a fresh
   user-controlled install path, verifies SHA-256 with the platform `shasum`
   fallback, installs executable launchers, and refuses quarantined payloads.
4. The alpha release checklist records the raw Mach-O layout and the explicit
   unsigned, non-notarized alpha policy.

## Planning Rule

macOS support is not announced until a real macOS machine has produced build, native test, install, `spio doctor`, managed `styio`, clean project workflow, and release artifact evidence.
