# Docs / Delivery Runbook

**Purpose:** Provide the daily-work entrypoint for `spio` docs tree, repo hygiene, docs gate, and delivery-facing workflow documentation.

**Last updated:** 2026-04-20

## Mission

Own docs topology, generated indexes, gate wiring, and delivery-facing entrypoints without redefining planning, registry, or compiler contract semantics.

## Owned Surface

1. `README.md`
2. `docs/`
3. `scripts/docs-index.py`
4. `scripts/docs-lifecycle.py`
5. `scripts/docs-audit.py`
6. `scripts/repo-hygiene-gate.py`
7. `scripts/team-docs-gate.py`
8. `scripts/docs-gate.sh`
9. `scripts/delivery-gate.sh`
10. `scripts/ecosystem-cli-doc-gate.py`

## Daily Workflow

1. Keep repository-level build, docs, and delivery entrypoints consistent.
2. Regenerate `INDEX.md` files after docs-tree changes.
3. Keep workflow docs in `docs/assets/workflow/` aligned with the actual scripts.
4. Keep the shared `styio-spio` / `styio-nightly` toolchain baseline explicit in docs and CI: Debian 13, LLVM 18.1.x, CMake/CTest 3.31.6, and Python 3.13.5.
5. Keep the official command grammar consistent across docs: `spio use <mode>`, `spio set <subject> as <value>`, and `spio cloud status --json`.

## Change Classes

1. Small: link fixes, README cleanup, or index refreshes.
2. Medium: docs tree, gate wiring, workflow entrypoint changes, or cloud-control-plane contract updates.
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

## Handoff / Recovery

Record the affected owner docs, generated indexes still pending, and which gate or workflow entrypoint still needs follow-up.
