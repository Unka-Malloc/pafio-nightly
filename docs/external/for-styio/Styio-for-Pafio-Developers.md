# Styio for Pafio Developers

**Purpose:** Define the public process and machine-contract boundary Pafio uses to invoke Styio.

**Last updated:** 2026-09-05

Styio owns the language, parser, analyzer, compiler, diagnostics, receipts, and
runtime events. Pafio treats Styio as an external executable and never imports
compiler source or private headers.

## Discovery and Probe

Pafio discovers Styio in this order:

1. command-local `--styio-bin`
2. `PAFIO_STYIO_BIN`
3. `styio` on `PATH`

It probes:

```text
styio --machine-info=json
```

The response must identify `styio`, advertise compatible compile-plan
versions, and provide the required capabilities and edition bound.

## Workflow Handoff

After dependency sync and plan validation, Pafio invokes:

```text
styio --compile-plan <path>
```

Pafio records workflow intent and process status. It does not reinterpret or
republish Styio's diagnostics, receipt, or runtime-event schemas.

Optional compiler emissions are requested through the plan's `emit` object.
`--emit-observable-static-snapshot[=<schema-version>]` with repeatable
`--observable-capability <name>` adds `emit.observable_static_snapshot`; Styio
owns its validation, the snapshot artifact, and the receipt entry that names it.
`--observable-parent-snapshot <path>` (requires the emission flag) adds
`emit.observable_static_snapshot.parent_snapshot_path`, a transport path to the
previous snapshot that never affects the cache key; Styio decides whether it can
write `<output-stem>.observable-delta.json` beside the snapshot or must record a
`full_snapshot_required` reason in `<plan.build_root>/receipt.json`.
Ordinary workflows never send the field.

Contract changes must be implemented and tested by Styio first, then consumed
by Pafio through a cross-repository interoperability fixture.
