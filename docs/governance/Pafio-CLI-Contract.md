# Pafio CLI Contract

**Purpose:** Freeze Pafio's terminal entry, external Styio discovery, and stable machine contracts.

**Last updated:** 2026-07-30

## Commands

```text
pafio [-h|--help] [--version] [--json] <command> [arguments]

new  init  doctor  metadata  add  remove  sync  tree
check  build  run  test  vendor  pack  publish
registry trust  machine-info
```

`install`, `use`, `set`, `tool`, `cloud`, `fetch`, `lock`, and `project-graph`
are not commands and have no aliases.

## Workflow transaction

`check`, `build`, `run`, and `test` execute the same sync transaction before
compiler validation or invocation. `--locked` forbids lock mutation, `--offline`
forbids network access, and `--frozen` enables both.

## External Styio

Discovery order is command-local `--styio-bin`, `PAFIO_STYIO_BIN`, then `styio`
on `PATH`. Pafio validates the machine contract but never installs, updates,
switches, pins, builds, repairs, or caches Styio.

## Machine contracts

`pafio metadata --json` emits metadata v1 and contains only package, workspace,
dependencies, targets, lock, resolution, and vendor state.

`pafio --json check|build|run|test` emits a stable envelope containing action,
target intent, status, sync result, and Styio process status. Styio owns concrete
diagnostic, receipt, and runtime-event schemas.

`pafio doctor` is read-only and diagnoses project state, cache, registry trust,
and external Styio capability.
