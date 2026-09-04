# Styio / Contracts Runbook

**Purpose:** Route maintenance for Pafio's external Styio and machine-contract handoffs.

**Last updated:** 2026-09-05

## Mission

Keep compiler discovery and compile-plan production aligned with Styio's
published contracts while leaving compiler lifecycle and output schemas to
Styio.

## Owned Surface

1. `contracts/compile-plan/` and `contracts/compat/`
2. `docs/external/for-styio/`
3. `docs/governance/Pafio-CLI-Contract.md`
4. `scripts/styio-interface-gate.py`
5. `scripts/preflight-readiness-check.py`
6. `contracts/ecosystem/owner-matrix.json`
7. `scripts/verify-ecosystem-contracts.py`

## Daily Workflow

1. Validate discovery precedence: flag, environment, then `PATH`.
2. Probe `styio --machine-info=json` before workflow delegation.
3. Keep `generated_by.tool = "pafio"` in compile plans.
4. Treat diagnostics, receipts, and runtime events as Styio-owned payloads.
5. Replay semantic checks from immutable owner commits before executing the
   Pafio, Styio, and Vityo product composition.
6. Keep the global `-h` and `--help` spellings in the published CLI contract
   aligned with Pafio's executable help surface.

## Change Classes

1. Small: handoff documentation or error wording.
2. Medium: capability, compatibility range, or compile-plan producer change.
3. High: machine-contract major version or ownership boundary.

## Required Gates

```bash
python3 scripts/styio-interface-gate.py --styio-bin <styio> --pafio-bin <pafio> --require-compile-plan --json
bash tests/interop/styio-interface-gate-handshake.sh
bash tests/interop/styio-interface-gate-compile-plan.sh
```

## Cross-Team Dependencies

Core / Workflow reviews workflow effects. Styio owns the consumer-side contract
and interoperability fixture. Docs / Delivery reviews public wording.

## Handoff / Recovery

Record contract versions, required capabilities, stable failure code, and the
owner repository that must act next.

2026-09-04: Recorded a dormant observable-identity handoff for Styio. Existing
`compile-plan v1` package names, target selection, and package-relative entry
paths are the first identity inputs; absolute paths and semantic identifiers
remain outside Pafio ownership. No schema, capability, fixture, or executable
change is scheduled unless accepted Styio snapshot fixtures prove a concrete
ambiguity.

2026-09-05: Added the additive optional `emit.observable_static_snapshot`
object (`schema_version`, `required_capabilities`) to `compile-plan v1` as a
pure emission passthrough for Styio PLAN-004. Pafio forwards the request only
when a caller opts in; Styio validates it and lists the snapshot artifact in
its receipt. The identity handoff above remains dormant.

2026-09-05: Added the additive optional `parent_snapshot_path` string to that
object for Styio stage S2 delta emission. It is a transport path, not identity,
and is excluded from the cache key; Styio decides between writing
`<output-stem>.observable-delta.json` and recording `full_snapshot_required` in
its receipt. Pafio never reads the parent or the delta.

2026-09-05: Added the additive optional `emit.runtime_observation` object
(`version` plus caller-set `mode`, `required_capabilities`, `lane_capacity`,
`priority_reserved`, `producer_lanes`, `sampling`) to `compile-plan v1` as a
pure passthrough for Styio stage S3 runtime-events v2. Pafio applies no defaults;
Styio owns the version, capability, bound, and sampling validation, writes the
receipt-named runtime-events JSONL artifact, and lists it in its receipt. Pafio
never reads runtime events.
