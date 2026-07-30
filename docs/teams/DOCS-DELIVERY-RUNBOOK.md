# Docs / Delivery Runbook

**Purpose:** Keep Pafio product contracts, generated indexes, planning state, and delivery evidence aligned.

**Last updated:** 2026-07-30

## Mission

Maintain one current product model across repository entry docs, governance, ADRs,
the Better Plan workspace, and release evidence.

## Owned Surface

1. Repository README and policy documents.
2. `docs/` collections and generated indexes.
3. Documentation, lifecycle, audit, hygiene, and delivery gates.
4. Public handoff documentation after executable owner contracts pass.
5. Fixed-revision owner-matrix and coordinated release evidence.

## Daily Workflow

1. Change the normative governance contract before its summaries.
2. Record durable ownership changes in an ADR.
3. Keep `docs/plan/Manifest.json` limited to current delivery and completed evidence.
4. Regenerate indexes after the docs tree changes.
5. Keep the public Pafio command inventory aligned with executable help.
6. Publish no site or release wording before the fixed-revision product matrix passes.
7. Remove superseded owner documents during a clean break; do not retain legacy
   names, compatibility notes, or permanent migration gates as active policy.
8. Keep post-commit instructions on public executable gates and repository-
   relative placeholders; never publish private machine paths or removed
   sibling-repository script entrypoints.

## Change Classes

1. Small: link, wording, or generated-index refresh.
2. Medium: contract, plan topology, gate, or handoff update.
3. High: product ownership, public command, schema, or release-boundary change.

## Required Gates

```bash
python3 scripts/docs-audit.py
python3 scripts/repo-hygiene-check.py --mode tracked
git diff --check
```

Run product-focused and final release gates through the active Better Plan rather
than duplicating their commands here.

## Cross-Team Dependencies

1. Core / Workflow reviews terminal and local workflow behavior.
2. Registry / Publish reviews client-versus-Platform ownership.
3. Styio / Contracts reviews compiler and machine-contract handoffs.
4. Platform and Vityo owners review their own adapters before public documentation.

## Handoff / Recovery

Record the affected owner contract, last passing focused gate, generated indexes
still pending, and the exact revision that needs repair. Never include credentials,
private machine paths, user information, or backend runtime payloads.
