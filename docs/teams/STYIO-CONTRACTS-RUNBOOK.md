# Styio / Contracts Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` maintainers of external compiler contracts, compatibility boundaries, and compiler-facing handoff docs.

**Last updated:** 2026-04-20

## Mission

Own `spio`'s published external compiler contract for `binary` mode and the controlled source-build handoff rules for `build` mode without letting either path drift into undocumented behavior.

## Owned Surface

1. `contracts/`
2. `docs/styio/`
3. `docs/governance/Spio-CLI-Contract.md`
4. `scripts/styio-interface-gate.py`
5. `scripts/preflight-readiness-check.py`

## Daily Workflow

1. Treat published compiler interaction as a machine contract, not an internal source dependency.
2. Treat source-build mode as a separate documented contract with explicit source origin, branch-channel mapping, revision, and cache rules.
3. Keep handoff docs and interface gates aligned in the same checkpoint.
4. Use `--styio-bin` health legs when validating the published binary path.

## Change Classes

1. Small: compatibility doc wording or fixture updates.
2. Medium: handshake fields, compile-plan consumer expectations, source-build fetch rules, or CLI JSON contract updates.
3. High: compatibility phase changes, official source origin rules, or public machine contract expansion.

## Required Gates

```bash
./scripts/checkpoint-health.sh --styio-bin /absolute/path/to/styio
```

## Cross-Team Dependencies

1. Core / Workflow reviews changes that alter build/check/run/test orchestration or project-local toolchain state behavior.
2. Docs / Delivery reviews cross-repo doc or gate entrypoint changes.

## Handoff / Recovery

Record the exact published external compiler binary or source revision, compatibility phase when relevant, and the failing contract command when stopping mid-checkpoint.
