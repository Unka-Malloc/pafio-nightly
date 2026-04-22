# Registry / Publish Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` registry and publish maintainers covering registry docs, publish/fetch flows, and promotion tooling.

**Last updated:** 2026-04-23

## Mission

Own registry transport and publish/promotion behavior without redefining core workflow semantics or external compiler contracts.

## Owned Surface

1. `docs/registry/`
2. `scripts/registry-promote.py`
3. `scripts/registry-server-gate.py`

## Daily Workflow

1. Keep registry transport, promotion behavior, registry-v2 static protocol, and control-plane contract docs aligned in `docs/registry/`.
2. Keep acceptance commands discoverable from the verification matrix and checkpoint health docs.
3. Coordinate with Core / Workflow when publish/fetch behavior changes user-facing workflow outcomes.
4. Keep `RegistryHttpTransport` as a transport-only strategy boundary. Registry semantics stay in `RemotePublish` / publish domain code, and external process execution stays in `SpioCore::Process`.

## Change Classes

1. Small: local registry test or runbook cleanup.
2. Medium: registry layout, publish semantics, control-plane contract, or promotion flow updates.
3. High: split-origin, auth-adjacent, hosted deployment model, or publish authorization changes.

## Required Gates

```bash
./scripts/checkpoint-health.sh
```

## Cross-Team Dependencies

1. Core / Workflow reviews changes that alter user-facing publish/fetch commands.
2. Docs / Delivery reviews changes to gate docs or delivery entrypoints.

## Handoff / Recovery

Record the registry mode affected, the acceptance command that still fails, whether local or remote storage assumptions changed, and which registry-v2 or control-plane contract fixture was last known good.
