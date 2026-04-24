# Core / Workflow Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` core workflow maintainers covering the native CLI, manifests, lockfiles, resolver, offline package paths, local import/export, and build/test flow.

**Last updated:** 2026-04-24

## Mission

Own the local-first package-manager core and native workflow behavior, including
offline package use, local import/export, project-local toolchain mode
selection, source-build orchestration, and platform compatibility shims, without
redefining registry policy, platform service behavior, or published external
compiler contracts.

## Owned Surface

1. `src/`
2. `tests/`
3. `CMakeLists.txt`
4. `scripts/bootstrap-check.py`
5. `scripts/native-check.sh`
6. `scripts/checkpoint-health.sh`
7. offline resolver/cache behavior and future local import/export commands
8. Temporary platform compatibility surfaces such as `spio cloud` until they are consumed from `styio-platform`

## Daily Workflow

1. Start from [../BUILD-AND-DEV-ENV.md](../BUILD-AND-DEV-ENV.md).
2. Keep native build/test behavior behind `scripts/checkpoint-health.sh`.
3. Treat `spio use`, `spio set`, `spio sync`, `spio project-graph --json`, `spio tool status --json`, `spio build minimal`, local import/export, offline package use, and `spio-toolchain.lock` as owned workflow surface.
4. Update CLI or workflow docs when public behavior changes.
5. Keep `src/SpioCLI/CLI.cpp` thin. New payload builders, workflow validation rules, and private process helpers belong in domain or infrastructure modules, not in the CLI router.
6. Keep `src/SpioToolchain/` vocabulary and project-local state terminology aligned with docs. When source-build, channel, risk, lane, or security terms change in code, update the owning governance and delivery docs in the same checkpoint.
7. Keep any remaining local cloud compatibility deterministic and small; new cloud stress or scheduler behavior belongs in `styio-platform`.

## Change Classes

1. Small: local command behavior, fixture cleanup, or dry-run plan output. Run checkpoint health.
2. Medium: CLI shape, manifest/lock semantics, dependency preparation loops, local import/export bundle semantics, project-graph payloads, platform compatibility payloads, tool-status payloads, toolchain-mode persistence, or resolver behavior. Update docs and tests together.
3. High: binary/build execution routing, source-build fetch/build semantics, offline package guarantees, platform compatibility semantics, or checkpoint entrypoint change. Coordinate with Docs / Delivery and Styio / Contracts.

## Required Gates

```bash
./scripts/checkpoint-health.sh
```

## Cross-Team Dependencies

1. Styio / Contracts reviews published-binary compatibility behavior and source-build contract wording.
2. Registry / Publish reviews changes that affect publish/fetch source semantics.
3. Docs / Delivery reviews workflow entrypoint or gate shape changes.

## Handoff / Recovery

Record the affected command path, fixture or test coverage gap, the selected project toolchain mode, and whether the next recovery step needs a published external `styio` binary, a source-build checkout, or a downstream `styio-platform` change.
