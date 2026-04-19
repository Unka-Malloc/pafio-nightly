# Styio / Contracts Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` maintainers of external compiler contracts, compatibility boundaries, and compiler-facing handoff docs.

**Last updated:** 2026-04-19

## Mission

Own `spio`'s black-box relationship to `styio` without creating source-level dependency drift.

## Owned Surface

1. `contracts/`
2. `docs/styio/`
3. `docs/governance/Spio-CLI-Contract.md`
4. `scripts/styio-interface-gate.py`
5. `scripts/preflight-readiness-check.py`

## Daily Workflow

1. Treat compiler interaction as a published machine contract, not an internal source dependency.
2. Keep handoff docs and interface gates aligned in the same checkpoint.
3. Use `--styio-bin` health legs when validating a real external compiler.

## Change Classes

1. Small: compatibility doc wording or fixture updates.
2. Medium: handshake fields, compile-plan consumer expectations, or CLI JSON contract updates.
3. High: compatibility phase changes or public machine contract expansion.

## Required Gates

```bash
./scripts/checkpoint-health.sh --styio-bin /absolute/path/to/styio
```

## Cross-Team Dependencies

1. Core / Workflow reviews changes that alter build/check/run/test orchestration.
2. Docs / Delivery reviews cross-repo doc or gate entrypoint changes.

## Handoff / Recovery

Record the exact external compiler binary, compatibility phase, and failing contract command when stopping mid-checkpoint.
