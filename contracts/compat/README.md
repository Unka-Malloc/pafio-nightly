# pafio Compatibility Matrix

**Purpose:** Record the Styio compiler ranges and protocol requirements that a given `pafio` release claims to support.

**Last updated:** 2026-10-07

## Rules

- `pafio` publishes after the `styio` releases it supports.
- A support entry must refer only to released compiler versions.
- Capability and contract checks still win over plain version ranges.
- This file is an implementation input, not just documentation.

## Explicit local compiler admission

`runtime_requirements` is the mandatory runtime contract profile, independent of
published product ranges: compile-plan v1, the listed capabilities, and the
edition ceiling remain required. Row-local copies are retained for older matrix
consumers; keep them and the embedded matrix in `src/PafioCompat/Compat.cpp` in
sync when the runtime profile changes.

A nonempty `--styio-bin` or `PAFIO_STYIO_BIN` explicitly selects a compiler.
After its machine contract passes, an unlisted, valid `compiler_version` or
nonempty `channel` is admitted with a product-support advisory. Automatic PATH
discovery still requires a published matrix match. Missing, malformed, or
incompatible machine information is always an error, with no fallback to another
binary. Product versions retain the handshake's strict `x.y.z` shape.

Reports distinguish `product_support` (`published` or `unlisted`) from
`selection_source` (`command_line`, `environment`, or `path`).
`release_provenance: "unverified"` is explicit: neither a machine self-report nor
a published range match verifies a release signature or certifies a toolchain.
Every non-dry-run workflow invokes the selected compiler and requires a fresh
receipt, invalidating only the prior receipt immediately before launch. This
does not change compile-plan v1 or establish trust in the executable, its
dynamically loaded dependencies, or wrapper tools.
