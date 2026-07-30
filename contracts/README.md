# spio Contracts

**Purpose:** Hold the source-of-truth machine contracts owned by the `spio` side of the `spio` / `styio` boundary.

**Last updated:** 2026-04-21

## Rules

- Files here are versioned public machine contracts.
- `styio` may vendor snapshots from here, but must not become the source of truth for them.
- Any breaking change requires a new versioned directory, not an in-place rewrite.

## Contents

- `compile-plan/` — build orchestration contract from `spio` to `styio`
- `compat/` — supported compiler matrix declarations used by `spio`
- `metadata-v1/` — canonical project metadata consumed by editor and automation clients
- `resolution-v1/` — resolved package roots bound to manifest and lock digests
- `registry-v2/` — industrial static package-distribution contract with signed metadata and append-only package indexes

Generated third-party API-description artifacts are not contract sources in this
tree. Compatibility is proven from the versioned JSON packages and examples.
