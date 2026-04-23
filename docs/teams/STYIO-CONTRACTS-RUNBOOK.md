# Styio / Contracts Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` maintainers of external compiler contracts, compatibility boundaries, and compiler-facing handoff docs.

**Last updated:** 2026-04-23

## Mission

Own `spio`'s published external compiler contract for `binary` mode and the controlled source-build handoff rules for `build` mode without letting either path drift into undocumented behavior.

## Owned Surface

1. `contracts/`
2. `docs/external/for-styio/`
3. `docs/governance/Spio-CLI-Contract.md`
4. `scripts/styio-interface-gate.py`
5. `scripts/preflight-readiness-check.py`

## Daily Workflow

1. Treat published compiler interaction as a machine contract, not an internal source dependency.
2. Treat source-build mode as a separate documented contract with explicit source origin, branch-channel mapping, revision, cache rules, and cloud execution-policy semantics, and keep machine-readable sync/graph/tool-status/cloud-plan entrypoints aligned with that contract vocabulary.
3. Keep handoff docs and interface gates aligned in the same checkpoint.
4. Use `--styio-bin` health legs when validating the published binary path.
5. Treat compile-plan v1 as live only when `styio --machine-info=json`, `contracts/compat/styio-support.toml`, and the black-box interop gate all agree.
6. Keep the source-build doc needles exact for the cross-repo gate: official origin, `stable`/`nightly` branch mapping, `spio build minimal`, `spio-toolchain.lock`, and the binary compatibility-matrix bypass statement must all remain visible in `Spio-CLI-Contract.md`.

## Change Classes

1. Small: compatibility doc wording, source-build needle wording, or fixture updates.
2. Medium: handshake fields, compile-plan consumer expectations, source-build fetch rules, cloud policy JSON fields, or CLI JSON contract updates.
3. High: compatibility phase changes, official source origin rules, public machine contract expansion, or cloud execution-policy vocabulary changes.

## Required Gates

```bash
./scripts/checkpoint-health.sh --styio-bin /absolute/path/to/styio
python3 scripts/styio-interface-gate.py --styio-bin /absolute/path/to/styio --spio-bin ./build-codex/bin/spio --require-compile-plan --json
```

## Cross-Team Dependencies

1. Core / Workflow reviews changes that alter build/check/run/test orchestration or project-local toolchain state behavior.
2. Docs / Delivery reviews cross-repo doc or gate entrypoint changes.

## Handoff / Recovery

Record the exact published external compiler binary or source revision, compatibility phase, supported compile-plan versions, and the failing contract command when stopping mid-checkpoint.
