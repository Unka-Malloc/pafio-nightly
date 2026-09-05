# Docs / Delivery Runbook

**Purpose:** Keep Pafio product contracts, generated indexes, planning state, and delivery evidence aligned.

**Last updated:** 2026-09-06

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
3. Keep `docs/plan/Capabilities.json` limited to observed or explicitly designed
   repository facts, and keep delivery lifecycle history in `Manifest.json` plus
   each group's `Checkpoints.json`.
4. Require each delivery group to retain one design node, one or more
   implementation nodes, and one trailing full-validation node; dependencies live
   only in node prerequisites.
5. Regenerate indexes after the docs tree changes.
6. Keep the public Pafio command inventory aligned with executable help.
7. Publish no site or release wording before the fixed-revision product matrix passes.
8. Remove superseded owner documents during a clean break; do not retain legacy
   names, compatibility notes, or permanent migration gates as active policy.
9. Keep post-commit instructions on public executable gates and repository-
   relative placeholders; never publish private machine paths or removed
   sibling-repository script entrypoints.
10. When the fixed-revision product matrix passes, close the Better Plan and gap
   ledger together. Keep branch promotion as an explicit maintainer handoff
   unless repository promotion is separately authorized.
11. Name the exact platform and source revision in release wording. Treat Linux,
    macOS, and Windows artifacts as independent publications; never infer one
    platform's readiness from another platform's gate or from shared `nightly`
    branch promotion.
12. Keep the fresh-host bootstrap aligned with every delivery tool used by CI,
    including `rsync` for extractability and exported-tree checks.
13. Keep documented global help aliases aligned with executable CLI output and
    its short-form regression test.

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

2026-09-04: Added a single-purpose conditional handoff plan under
`docs/external/for-styio/` and refreshed the generated indexes. The document
records why no Pafio implementation is currently needed and defines the fixture
evidence required before a separate delivery could be authorized.

2026-09-05: Documented the opt-in observable static snapshot emission
passthrough in the CLI contract, the compile-plan v1 contract README, and the
Styio handoff docs, and refreshed the generated indexes. The consumer locates the
Styio receipt and snapshot artifact through the existing `plan` envelope fields.

2026-09-06: Stopped empty generated indexes from stamping today's date, which
made `docs/plan/INDEX.md` and `docs/security/INDEX.md` fail CI across timezones.
Empty collections now inherit `README.md`'s last-updated date, and Better Plan
group directories are indexed through `Architecture.md` when they have no
`README.md` or `INDEX.md`.
