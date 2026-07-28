# Windows Compatibility Plan

**Purpose:** Manage Windows-specific compatibility work for `spio`, including native process execution, path and tool discovery, PowerShell installation, managed Styio installation, release artifacts, CI promotion, and real-machine validation evidence.

**Last updated:** 2026-06-28

## State Files

- `../Manifest.json` indexes this plan.
- `Checkpoints.json` is the executable checkpoint graph for Windows compatibility.
- Shared evidence format lives in [../spio-foundation/Evidence-Template.md](../spio-foundation/Evidence-Template.md).

## Target Matrix

1. `windows-x86_64` for Windows amd64.

## Current Evidence Snapshot

1. `local-ci-gate` includes a `windows-latest` native x64 build smoke.
2. The Windows CI lane is currently non-blocking because the native process backend, portable tests, and Windows installer are not complete.
3. Release target docs map `windows-*` to `styio-windows-cli`.

## Planning Rule

Windows support is not announced until a real Windows amd64 machine has produced build, native test, install, `spio doctor`, managed `styio`, clean project workflow, and release artifact evidence.
