# compile-plan v1

**Purpose:** Define the first machine-readable build plan that Pafio emits and Styio consumes through `--compile-plan`.

**Last updated:** 2026-04-12

## Source of Truth

- `compile-plan.schema.json` is the canonical schema file.
- `generated_by.tool` is exactly `pafio`; compiler channel data comes from the
  validated Styio machine contract rather than the project manifest.
- Human-facing design notes remain secondary to the schema.
- `package.targets.tests` is an additive optional field in `v1`; older plans may omit it when no explicit package tests exist.

## Stability Rules

- Additive optional fields are allowed within `v1`.
- Breaking field removals, required-field changes, or semantic rewrites require `v2`.
- Paths in concrete plans are expected to be absolute and ephemeral.
