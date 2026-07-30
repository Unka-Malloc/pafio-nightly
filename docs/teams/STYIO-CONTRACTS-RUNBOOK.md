# Styio / Contracts Runbook

**Purpose:** Route maintenance for Pafio's external Styio and machine-contract handoffs.

**Last updated:** 2026-07-30

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
