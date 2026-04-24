# registry-control-plane v1

**Purpose:** Define the first native JSON HTTP control-plane package for operating a `spio` registry `v2` root over a backend service boundary.

**Last updated:** 2026-04-24

## Source Of Truth

- `registry-control-plane.contract.json` is the canonical status, publish, and verify operation catalog.
- `registry-control-plane.examples.json` is the canonical example pack.

## Stability Rules

- Additive optional request fields are allowed within `v1`.
- Renaming operations, changing required fields, or changing envelope semantics requires `v2`.
- Clients must treat undocumented fields as non-existent.
- Services must preserve the published method, path, and envelope spelling exactly.
