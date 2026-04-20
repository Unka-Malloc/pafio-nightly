# Core / Workflow Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` core workflow maintainers covering the native CLI, manifests, lockfiles, resolver, and build/test flow.

**Last updated:** 2026-04-20

## Mission

Own the package-manager core and native workflow behavior, including project-local toolchain mode selection and source-build orchestration, without redefining registry policy or published external compiler contracts.

## Owned Surface

1. `src/`
2. `tests/`
3. `CMakeLists.txt`
4. `scripts/bootstrap-check.py`
5. `scripts/native-check.sh`
6. `scripts/checkpoint-health.sh`

## Daily Workflow

1. Start from [../BUILD-AND-DEV-ENV.md](../BUILD-AND-DEV-ENV.md).
2. Keep native build/test behavior behind `scripts/checkpoint-health.sh`.
3. Treat `spio use`, `spio set`, `spio project-graph --json`, `spio cloud status --json`, `spio cloud plan --json`, `spio tool status --json`, `spio build minimal`, and `spio-toolchain.lock` as owned workflow surface.
4. Update CLI or workflow docs when public behavior changes.
5. Keep `src/SpioCLI/CLI.cpp` thin. New payload builders, workflow validation rules, and private process helpers belong in domain or infrastructure modules, not in the CLI router.

## Change Classes

1. Small: local command behavior, fixture cleanup, or dry-run plan output. Run checkpoint health.
2. Medium: CLI shape, manifest/lock semantics, project-graph payloads, cloud-plan request payloads, tool-status payloads, toolchain-mode persistence, cloud preference persistence, or resolver behavior. Update docs and tests together.
3. High: binary/build execution routing, source-build fetch/build semantics, cloud execution policy semantics, or checkpoint entrypoint change. Coordinate with Docs / Delivery and Styio / Contracts.

## Required Gates

```bash
./scripts/checkpoint-health.sh
```

## Cross-Team Dependencies

1. Styio / Contracts reviews published-binary compatibility behavior and source-build contract wording.
2. Registry / Publish reviews changes that affect publish/fetch source semantics.
3. Docs / Delivery reviews workflow entrypoint or gate shape changes.

## Handoff / Recovery

Record the affected command path, fixture or test coverage gap, the selected project toolchain mode, and whether the next recovery step needs a published external `styio` binary or a source-build checkout.
