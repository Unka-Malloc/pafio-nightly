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
- `emit.runtime_observation` is an additive optional object in `v1` for Styio
  stage S3 runtime and scheduler correlation (runtime-events v2). It is absent
  unless a caller opts in through the `check`, `build`, `run`, or `test` flag
  `--emit-runtime-observation[=<version>]`; then it carries `version` (integer,
  minimum 1, default 2) plus only the fields the caller set: `mode` (one of
  `disabled`, `aggregate`, `sampled`, `detailed`, from
  `--runtime-observation-mode`), `required_capabilities` (sorted, unique strings
  from repeated `--runtime-observation-capability <name>`), `lane_capacity`
  (positive integer from `--runtime-observation-lane-capacity <n>`),
  `priority_reserved` and `producer_lanes` (positive integers, library-only),
  and `sampling` (`{numerator, denominator[, seed]}` from
  `--runtime-observation-sampling <numerator>/<denominator>[@<seed>]`). Pafio
  only forwards the request and applies no defaults; Styio owns every default
  (mode `aggregate`, capacity 256 with 32 reserved, sampling 1/16 seed 0), the
  supported version, the capability names, the power-of-two capacity bounds, and
  the sampling ratio, and rejects anything else before execution. The request is
  part of the cache key, so an observed run gets its own `outputs.build_root`;
  plans without the field are byte-identical to earlier `v1` output. Styio writes
  the runtime-events v2 JSONL artifact under the receipt-named output stem and
  lists it in `<plan.build_root>/receipt.json`; Pafio does not read it.

## Stability Rules

- Additive optional fields are allowed within `v1`.
- Breaking field removals, required-field changes, or semantic rewrites require `v2`.
- Paths in concrete plans are expected to be absolute and ephemeral.
