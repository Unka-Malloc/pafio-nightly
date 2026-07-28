# Linux Compatibility Plan

**Purpose:** Manage Linux-specific compatibility work for `spio`, including glibc and musl targets, distro-family installer adapters, real-machine validation, and Linux release evidence.

**Last updated:** 2026-06-28

## State Files

- `../Manifest.json` indexes this plan.
- `Checkpoints.json` is the executable checkpoint graph for Linux compatibility.
- Shared evidence format lives in [../spio-foundation/Evidence-Template.md](../spio-foundation/Evidence-Template.md).

## Target Matrix

1. `linux-aarch64` for glibc Linux on ARM64.
2. `linux-musl-aarch64` for musl Linux on ARM64.
3. `linux-x86_64` for glibc Linux on Debian, Red Hat family, openSUSE, and Arch family systems.
4. `linux-musl-x86_64` for musl Linux on x86_64, including Alpine.

## Current Evidence Snapshot

1. The blocking repository CI gate runs inside `debian:trixie`.
2. `scripts/install-spio.sh --print-adapter` covers Debian, Red Hat family, openSUSE, Arch family, and Alpine x86_64 smoke cases.

## Planning Rule

Linux support is not announced for any platform key until real-machine or equivalent VM evidence exists for native build, tests, installer, `spio doctor`, managed `styio`, and clean project workflow smoke.
