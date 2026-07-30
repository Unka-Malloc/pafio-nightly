# Pafio Version Decoupling Constraints

**Purpose:** Keep Pafio, Styio, language editions, and machine contracts independently versioned.

**Last updated:** 2026-07-30

## Independent Axes

1. Styio language edition
2. Styio compiler version
3. compile-plan contract version
4. Pafio product version
5. Pafio manifest, lock, metadata, and resolution contract versions

Pafio never infers compatibility from matching product versions. It probes
`styio --machine-info=json`, validates the advertised compile-plan version and
capabilities, and then delegates compilation through the published process
boundary.

Pafio must not link compiler internals, inspect compiler source, install or
switch the compiler, or reuse compiler outputs across incompatible compiler,
contract, edition, profile, target, or source identities.

`pafio.lock` changes only for dependency or lock-schema reasons; the Pafio
binary version is not a semantic dependency.

The package-manager compatibility declaration is
`contracts/compat/styio-support.toml`. Styio remains the authority for
diagnostics, receipts, and runtime events.
