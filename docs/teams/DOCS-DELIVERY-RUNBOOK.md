# Docs / Delivery Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` docs tree, repo hygiene, docs gate, and delivery-facing workflow documentation.

**Last updated:** 2026-04-24

## Mission

Own docs topology, generated indexes, gate wiring, delivery-facing entrypoints,
offline package docs, local import/export docs, and platform handoff docs
without redefining planning, registry, compiler, or service contract semantics.

## Owned Surface

1. `README.md`
2. `docs/`
3. `docs/external/`
4. `scripts/docs-index.py`
5. `scripts/docs-lifecycle.py`
6. `scripts/docs-audit.py`
7. `scripts/repo-hygiene-gate.py`
8. `scripts/team-docs-gate.py`
9. `scripts/docs-gate.sh`
10. `scripts/delivery-gate.sh`
11. `scripts/ecosystem-cli-doc-gate.py`

## Daily Workflow

1. Keep repository-level build, docs, and delivery entrypoints consistent.
2. Regenerate `INDEX.md` files after docs-tree changes.
3. Keep workflow docs in `docs/assets/workflow/` aligned with the actual scripts.
4. Keep the shared `styio-spio` / `styio-nightly` toolchain baseline explicit in docs and CI: Debian 13, LLVM 18.1.x, CMake/CTest 3.31.6, and Python 3.13.5.
5. Keep the official command grammar consistent across docs: `spio use <mode>`, `spio set <subject> as <value>`, `spio sync`, `spio project-graph --json`, `spio cloud status --json`, `spio cloud plan --json`, and `spio tool status --json`.
6. Keep repo entry docs and closure docs aligned: `README.md`, `docs/BUILD-AND-DEV-ENV.md`, `docs/planning/Spio-Master-Plan.md`, `docs/planning/Spio-Stage-Review-and-Future-Features.md`, `docs/planning/Spio-Workstreams-and-TODOs.md`, `docs/operations/Spio-Verification-Matrix.md`, `docs/operations/Spio-Cloud-Compile-Stress-Runbook.md`, and `docs/operations/Spio-Repo-Split-Runbook.md` must agree on wrapper-vs-binary entrypoints, current implementation status, and root-relative command paths.
7. When `docs/governance/Spio-CLI-Contract.md` changes source-build wording, run the cross-repo ecosystem CLI doc gate from `styio-nightly` and keep its fixed source-build needles exact.
8. Keep [../specs/POST-COMMIT-CI-CHECKS.md](../specs/POST-COMMIT-CI-CHECKS.md) aligned with actual GitHub Actions monitoring practice whenever commit, push, or CI handoff rules change.
9. Keep sibling-repository handoff docs under `docs/external/for-*` or explicit planning handoff docs; do not recreate root-level external handoff collections.
10. Keep `docs/planning/Spio-Platform-Migration-Handoff.md` aligned with downstream `styio-platform` docs when server/platform ownership moves.
11. Keep `docs/governance/Spio-Local-Offline-Package-Contract.md` aligned with README and registry docs when offline package or local import/export wording changes.

## Change Classes

1. Small: link fixes, README cleanup, or index refreshes.
2. Medium: docs tree, `docs/external/` handoff routing, gate wiring, workflow entrypoint changes, source-build contract wording, offline package wording, local import/export wording, post-push CI checking rules, or platform handoff updates.
3. High: ownership boundary or delivery-floor policy changes.

## Required Gates

```bash
./scripts/docs-gate.sh
python3 scripts/repo-hygiene-gate.py --mode tracked
./scripts/delivery-gate.sh --mode checkpoint --skip-health
```

## Cross-Team Dependencies

1. Core / Workflow reviews workflow entrypoint changes.
2. Styio / Contracts reviews ecosystem doc or machine-contract wording changes.
3. Registry / Publish reviews registry runbook or gate changes.
4. Styio / Contracts reviews `styio-platform` handoff wording.

## Handoff / Recovery

Record the affected owner docs, generated indexes still pending, and which gate or workflow entrypoint still needs follow-up.
