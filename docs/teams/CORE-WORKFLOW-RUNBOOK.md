# Core / Workflow Runbook

**Purpose:** Route maintenance for Pafio manifests, dependency transactions, metadata, and local workflows.

**Last updated:** 2026-07-30

## Mission

Maintain the local-first Pafio product without assuming ownership of compiler
installation or hosted execution.

## Owned Surface

1. `src/PafioCLI/`, `src/PafioApp/`, and `src/PafioManifest/`
2. `src/PafioResolve/`, `src/PafioWorkflow/`, and `src/PafioPlan/`
3. `src/PafioCore/`, `src/PafioTree/`, and `src/PafioVendor/`
4. focused native and CLI tests

## Daily Workflow

1. Start from the CLI and manifest contracts.
2. Keep one sync transaction behind `check`, `build`, `run`, and `test`.
3. Keep metadata bounded to its seven v1 fields.
4. Exercise the smallest relevant native filter before cross-repository tests.

## Change Classes

1. Small: isolated validation or rendering fix.
2. Medium: manifest, lock, resolution, metadata, or workflow behavior.
3. High: public command, schema, atomicity, or external process boundary.

## Required Gates

```bash
cmake --build build-codex --target pafio pafio_native_tests
ctest --test-dir build-codex -R '<focused-pattern>' --output-on-failure
git diff --check
```

## Cross-Team Dependencies

Registry / Publish reviews source acquisition and package lifecycle changes.
Styio / Contracts reviews compile-plan and compiler discovery changes.
Docs / Delivery reviews public surface changes.

## Handoff / Recovery

Record the affected transaction, last passing focused test, and remaining owner
handoff. Do not record local paths, credentials, or backend runtime data.
