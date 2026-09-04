# Core / Workflow Runbook

**Purpose:** Route maintenance for Pafio manifests, dependency transactions, metadata, and local workflows.

**Last updated:** 2026-09-05

## Mission

Maintain the local-first Pafio product without assuming ownership of compiler
installation or hosted execution.

## Owned Surface

1. `src/PafioCLI/`, `src/PafioApp/`, and `src/PafioManifest/`
2. `src/PafioResolve/`, `src/PafioWorkflow/`, and `src/PafioPlan/`
3. `src/PafioCore/`, `src/PafioTree/`, and `src/PafioVendor/`
4. focused native and CLI tests
5. black-box acceptance for public workflow and ecosystem-verifier behavior

## Daily Workflow

1. Start from the CLI and manifest contracts.
2. Keep one sync transaction behind `check`, `build`, `run`, and `test`.
3. Keep metadata bounded to its seven v1 fields.
4. Exercise the smallest relevant native filter before cross-repository tests.
5. Keep the fixed-revision verifier acceptance isolated from live owner
   worktrees; validate executable new/build/frozen/metadata behavior only in the
   final full product matrix.
6. Exercise registry graph resolution with a minimal verified cache fixture.
   Do not revive filesystem publish or duplicate the Platform registry service
   to prepare repository-local tests.
7. Keep the top-level `-h` and `--help` aliases equivalent and cover both forms
   with executable CLI tests.

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

2026-09-05: `check`, `build`, `run`, and `test` accept the opt-in
`--emit-observable-static-snapshot[=<schema-version>]` and repeatable
`--observable-capability <name>` flags, which only add
`emit.observable_static_snapshot` to the compile plan. Plans without the flag
stay byte-identical; focused coverage lives in `BuildPlanTests` and
`BuildCliTests` plus the `pafio_cli_*_help` CTest entries.
