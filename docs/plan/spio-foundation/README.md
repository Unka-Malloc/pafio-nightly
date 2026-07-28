# Spio Foundation Plan

**Purpose:** Manage shared `spio` foundations required by Linux, macOS, and Windows compatibility: target keys, CI lane policy, evidence format, platform identity, portable tests, release artifact layout, and final support gate policy.

**Last updated:** 2026-06-28

## State Files

- `../Manifest.json` indexes this plan.
- `Checkpoints.json` is the executable checkpoint graph for shared `spio` foundations.
- `Evidence-Template.md` defines the per-machine report used by Linux, macOS, and Windows platform agents.

## Final Compatibility State

`spio` converges on one native package-manager implementation that works across Linux, macOS, and Windows. Platform-specific code is allowed only at OS responsibility boundaries: process execution, executable naming, path and environment conventions, installer entrypoints, release artifact lookup, and CI or real-machine validation.

## Shared Target Matrix

1. `darwin-aarch64` for Apple Silicon macOS.
2. `linux-aarch64` for glibc Linux on ARM64.
3. `linux-musl-aarch64` for musl Linux on ARM64.
4. `linux-x86_64` for glibc Linux on Debian, Red Hat family, openSUSE, and Arch family systems.
5. `linux-musl-x86_64` for musl Linux on x86_64, including Alpine.
6. `windows-x86_64` for Windows amd64.

## Related Platform Plans

1. Linux-specific work lives in [../linux-compatibility/README.md](../linux-compatibility/README.md).
2. macOS-specific work lives in [../macos-compatibility/README.md](../macos-compatibility/README.md).
3. Windows-specific work lives in [../windows-compatibility/README.md](../windows-compatibility/README.md).

## Planning Rule

A platform is not supported until a real machine or equivalent VM has produced build, native test, install, `spio doctor`, managed `styio`, and clean project workflow evidence for that platform key using the evidence template.
