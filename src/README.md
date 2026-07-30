# spio Source

**Purpose:** Hold the standalone `spio` implementation. This directory must remain independent from `styio` compiler internals.

**Last updated:** 2026-04-24

## Rule

- Do not add source-level dependencies on `styio` implementation files.
- The authoritative implementation path should move toward native `C++20` sources built by `CMake`.
- `src/` is the package-manager and project-workflow implementation surface.
- Keep the native core layered:
  - `SpioCLI/` owns thin command routing, shared usage/help text, and machine-info contracts.
  - `SpioApp/` owns project workflow and package lifecycle orchestration.
  - domain modules such as `SpioResolve/`, `SpioPlan/`, and `SpioPublish/` own typed contracts, offline package behavior, and validation rules.
  - `SpioPlan/` renders the external Styio compile-plan handoff.
  - `SpioCore/` owns shared infrastructure such as process execution and path policy.

## Registry Split

- `src/SpioRegistryClient/` owns registry v2 static read-plane consumption and local materialization, but not private trust/auth policy.
- `src/SpioSecurity/` owns the public registry-security interface boundary.
- `src/SpioPublish/` owns publish-candidate preparation and the bounded archive
  request sent to the platform-owned HTTP(S) control plane.
- Registry signing, validation, static-root mutation, and promotion are
  platform-owned server responsibilities and must not be implemented here.
- `src-private/` is gitignored and reserved for closed-source security implementations; public extractability excludes it.

## Implementation Notes

- Public JSON payloads must come from domain-owned serializer modules such as:
  - `SpioResolve/MetadataContract.*`
  - `SpioResolve/ResolutionContract.*`
  - `SpioCLI/MachineInfoContract.*`
- `SpioCLI/CLI.cpp` must remain a routing shell; it must not grow new payload builders, direct workflow validation, or private `fork/exec` helpers.
- All external process execution must go through `SpioCore/Process.*`.
