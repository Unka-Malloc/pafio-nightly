# Core / Workflow Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` core workflow maintainers covering the native CLI, manifests, lockfiles, resolver, and build/test flow.

**Last updated:** 2026-04-19

## Mission

Own the package-manager core and native workflow behavior without redefining registry policy or external compiler contracts.

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
3. Update CLI or workflow docs when public behavior changes.

## Change Classes

1. Small: local command behavior or fixture cleanup. Run checkpoint health.
2. Medium: CLI shape, manifest/lock semantics, or resolver behavior. Update docs and tests together.
3. High: workflow contract or checkpoint entrypoint change. Coordinate with Docs / Delivery and Styio / Contracts.

## Required Gates

```bash
./scripts/checkpoint-health.sh
```

## Cross-Team Dependencies

1. Styio / Contracts reviews compiler-facing behavior changes.
2. Registry / Publish reviews changes that affect publish/fetch source semantics.
3. Docs / Delivery reviews workflow entrypoint or gate shape changes.

## Handoff / Recovery

Record the affected command path, fixture or test coverage gap, and whether the next recovery step needs an external `styio` binary.
