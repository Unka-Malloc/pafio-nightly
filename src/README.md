# spio Source

**Purpose:** Hold the standalone `spio` implementation. This directory must remain independent from `styio` compiler internals.

**Last updated:** 2026-04-09

## Rule

- Do not add source-level dependencies on `styio` implementation files.
- The authoritative implementation path should move toward native `C++20` sources built by `CMake`.
- Python modules under `src/` are registry/control-plane tooling owned by `spio`; they are part of the active package-management implementation where referenced by CLI scripts.
- `src/` is the backend/service implementation surface. Human-facing control-console assets belong under `../frontend/console/` and must consume `spio` through published contracts, not direct source coupling.
- Keep the native core layered:
  - `SpioCLI/` owns thin command routing, shared usage/help text, and machine-info contracts.
  - `SpioApp/` owns command-cluster orchestration for workflow, cloud, tool, and package operations.
  - domain modules such as `SpioCloud/`, `SpioResolve/`, `SpioTool/`, and `SpioPublish/` own typed contracts and validation rules.
  - `SpioCore/` owns shared infrastructure such as process execution and path policy.

## Registry Split

- `src/SpioRegistryClient/` owns registry v2 static read-plane consumption and local materialization, but not private trust/auth policy.
- `src/spio_registry_v2/` owns registry v2 key generation, local publish, verification, and control-plane helpers used by scripts and tests.
- `src/SpioSecurity/` owns the public registry-security interface boundary.
- `src/SpioPublish/` owns shared publish-candidate preparation and must not absorb registry control-plane logic.
- `src-private/` is gitignored and reserved for closed-source security implementations; public extractability excludes it.

## Implementation Notes

- Public JSON payloads must come from domain-owned serializer modules such as:
  - `SpioCloud/Contract.*`
  - `SpioResolve/ProjectGraphContract.*`
  - `SpioTool/Contract.*`
  - `SpioCLI/MachineInfoContract.*`
- `SpioCLI/CLI.cpp` must remain a routing shell; it must not grow new payload builders, direct workflow validation, or private `fork/exec` helpers.
- All external process execution must go through `SpioCore/Process.*`.
