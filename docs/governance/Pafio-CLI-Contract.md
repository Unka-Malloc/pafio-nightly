# Pafio CLI Contract

**Purpose:** Freeze Pafio's terminal entry, external Styio discovery, and stable machine contracts.

**Last updated:** 2026-09-05

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
diagnostic, receipt, and runtime-event schemas. The envelope's `plan` object
names the compile-plan outputs: `plan.path`, `plan.build_root`,
`plan.artifact_dir`, `plan.diag_dir`, and `plan.cache_key`. Styio writes its
receipt to `<plan.build_root>/receipt.json` and its artifacts under
`plan.artifact_dir`; Pafio verifies those paths exist after a successful run but
does not read or republish them.

`pafio doctor` is read-only and diagnoses project state, cache, registry trust,
and external Styio capability.

## Observable static snapshot passthrough

`check`, `build`, `run`, and `test` accept the opt-in flag
`--emit-observable-static-snapshot[=<schema-version>]` (default schema version
`1`) and the repeatable `--observable-capability <name>`. When present, Pafio adds
`emit.observable_static_snapshot` with `schema_version` and the sorted, unique
`required_capabilities` to the compile plan and nothing else changes; without the
flag the compile plan is byte-identical to the ordinary plan. `--observable-capability`
without the emission flag, a non-positive or non-numeric schema version, or an
empty capability name is a `UsageError`.

Pafio does not check whether the discovered Styio supports the requested
snapshot; Styio rejects unsupported schema versions and capability names with its
own diagnostic, which surfaces as the ordinary `CompilerError` workflow failure.
A consumer that requested the snapshot reads `<plan.build_root>/receipt.json`
from the success envelope and follows the receipt's artifact list to the
`<output-stem>.observable-static-snapshot.json` file under `plan.artifact_dir`.
Pafio never mints snapshot identifiers, inspects snapshots, or adds identity
fields for them.

### Parent snapshot passthrough

The same four commands accept `--observable-parent-snapshot <path>`, which
requires `--emit-observable-static-snapshot` and adds
`emit.observable_static_snapshot.parent_snapshot_path` to the compile plan. The
value is a filesystem path to the previous snapshot artifact; Pafio passes it
through as an absolute, lexically normalized path (a relative value is resolved
against the plan's `workspace_root`), does not check that it exists, and keeps
it out of the cache key, so the request lands in the same `plan.build_root` as
the same request without a parent. The flag without the emission flag, a
missing value, or an empty value is a `UsageError`.

Styio owns the delta: with a readable, matching parent it writes
`<output-stem>.observable-delta.json` next to
`<output-stem>.observable-static-snapshot.json` and lists both in
`<plan.build_root>/receipt.json`; with an unreadable or mismatched parent it
still publishes the snapshot and records a `full_snapshot_required` reason in the
receipt. An IDE consumer finds both artifacts, or the reason, through that
receipt path from the success envelope. Pafio does not read the parent, the
delta, or the reason.
