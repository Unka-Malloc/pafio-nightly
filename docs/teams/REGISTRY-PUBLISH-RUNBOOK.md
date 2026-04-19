# Registry / Publish Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` registry and publish maintainers covering registry docs, publish/fetch flows, and promotion tooling.

**Last updated:** 2026-04-19

## Mission

Own registry transport and publish/promotion behavior without redefining core workflow semantics or external compiler contracts.

## Owned Surface

1. `docs/registry/`
2. `scripts/registry-promote.py`
3. `scripts/registry-server-gate.py`

## Daily Workflow

1. Keep registry transport and promotion behavior documented in `docs/registry/`.
2. Keep acceptance commands discoverable from the verification matrix and checkpoint health docs.
3. Coordinate with Core / Workflow when publish/fetch behavior changes user-facing workflow outcomes.

## Change Classes

1. Small: local registry test or runbook cleanup.
2. Medium: registry layout, publish semantics, or promotion flow updates.
3. High: split-origin, auth-adjacent, or deployment model changes.

## Required Gates

```bash
./scripts/checkpoint-health.sh
```

## Cross-Team Dependencies

1. Core / Workflow reviews changes that alter user-facing publish/fetch commands.
2. Docs / Delivery reviews changes to gate docs or delivery entrypoints.

## Handoff / Recovery

Record the registry mode affected, the acceptance command that still fails, and whether local or remote storage assumptions changed.
