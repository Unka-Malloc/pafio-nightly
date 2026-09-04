# compile-plan v1

**Purpose:** Define the first machine-readable build plan that Pafio emits and Styio consumes through `--compile-plan`.

**Last updated:** 2026-09-05

## Source of Truth

- `compile-plan.schema.json` is the canonical schema file.
- `generated_by.tool` is exactly `pafio`; compiler channel data comes from the
  validated Styio machine contract rather than the project manifest.
- Human-facing design notes remain secondary to the schema.
- `package.targets.tests` is an additive optional field in `v1`; older plans may omit it when no explicit package tests exist.
- `emit.observable_static_snapshot` is an additive optional field in `v1`. It is
  absent unless a caller opts in through the `check`, `build`, `run`, or `test`
  flag `--emit-observable-static-snapshot[=<schema-version>]`; then it carries
  `schema_version` (integer, minimum 1, default 1) and `required_capabilities`
  (sorted, unique strings filled from repeated `--observable-capability <name>`,
  possibly empty). Pafio only forwards the request; Styio validates the schema
  version and capability names, publishes the snapshot artifact, and lists it in
  its receipt. Plans without the field are byte-identical to earlier `v1` output.
- `emit.observable_static_snapshot.parent_snapshot_path` is an additive optional
  string (minimum length 1) inside that object, present only when the caller also
  passes `--observable-parent-snapshot <path>`. It is a transport input like
  `workspace_root`, never identity: it names the previous snapshot artifact and
  is written as an absolute, lexically normalized path (a relative value is
  resolved against `workspace_root`). It does not enter the cache key, so a plan
  with a parent shares its `outputs.build_root` with the same request without
  one. Pafio does not check that the file exists. Styio owns the consequence:
  with a readable, matching parent it also writes `<stem>.observable-delta.json`
  next to `<stem>.observable-static-snapshot.json` and lists it in the receipt;
  with an unreadable or mismatched parent it still publishes the snapshot and
  records a `full_snapshot_required` reason in the receipt.

## Stability Rules

- Additive optional fields are allowed within `v1`.
- Breaking field removals, required-field changes, or semantic rewrites require `v2`.
- Paths in concrete plans are expected to be absolute and ephemeral.
