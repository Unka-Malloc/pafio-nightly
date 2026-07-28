# Platform Compatibility Evidence Template

**Purpose:** Define the evidence record that each Linux, macOS, or Windows platform agent must produce before a `spio` platform target can be announced as supported.

**Last updated:** 2026-06-28

## Machine Identity

- Platform key:
- Operating system name and release:
- CPU architecture:
- libc family, for Linux:
- Shell or terminal used:
- CMake version:
- C++ compiler and version:
- Python version:
- Git version:

## Build And Test Evidence

Record exact commands, exit codes, and artifact paths for:

1. Native CMake configure.
2. Native CMake build.
3. Native test run.
4. `spio --version`.
5. `spio machine-info --json`.
6. `spio doctor --json`.

## Installer Evidence

Record exact commands, exit codes, and installed paths for:

1. Platform installer invocation.
2. Installed `spio` executable.
3. Installed `styio` shim or launcher.
4. Release-root configuration under `SPIO_HOME`.
5. SHA-256 verification result.

## Managed Styio Evidence

Record exact commands, exit codes, and managed tool paths for:

1. `spio install styio@latest`.
2. `styio --version`.
3. `spio tool status --json`.
4. Current managed compiler path.

## Clean Project Workflow Evidence

Record exact commands, exit codes, generated files, and stdout/stderr artifacts for:

1. `spio use binary`.
2. `spio sync`.
3. `spio build minimal`.
4. `spio run` or a documented package binary smoke.
5. `spio test` or a documented package test smoke.

## Failure Notes

Every failure must include:

1. The failing command.
2. Exit code.
3. Relevant stdout and stderr excerpt.
4. Whether the failure is an OS prerequisite issue, packaging issue, native code issue, release artifact issue, or documentation issue.
5. The smallest proposed remediation.
