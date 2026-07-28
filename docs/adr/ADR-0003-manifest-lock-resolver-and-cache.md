# ADR-0003: Manifest, Lockfile, Resolver, and Source Cache

**Purpose:** Record the functional decision that manifests, lockfiles, graph resolution, dependency editing, source fetching, and cache materialization form one deterministic package-manager core.

**Last updated:** 2026-06-28

## Status

Accepted.

## Supersedes

- `ADR-0004-phase2-canonical-manifest-lock-writeback.md`
- `ADR-0005-phase2-lock-command-local-graph-scope.md`
- `ADR-0006-phase2-lock-cli-and-local-identity.md`
- `ADR-0007-phase3-minimal-single-version-resolver.md`
- `ADR-0008-hermetic-git-snapshot-cache-under-spio-home.md`
- `ADR-0009-phase3-tree-renders-resolver-graph.md`
- `ADR-0010-phase3-basic-dependency-edit-and-fetch-commands.md`
- `ADR-0011-phase3-check-validates-resolver-graph-and-lock-drift.md`

## Context

A package manager cannot treat manifest parsing, lockfile generation, resolver behavior, and source materialization as unrelated features. If any of those layers has looser semantics than the others, `spio` can produce a lockfile that is syntactically valid but not semantically current.

The project now supports local workspaces, path dependencies, pinned git dependencies, and registry dependencies. It also has graph-aware commands that need a single view of package identity and source fingerprints.

## Decision

`spio.toml` and `spio.lock` use deterministic parsing, validation, and canonical write-back. Lock generation is resolver-backed, not a formatting-only operation.

The resolver uses the conservative `single-version-v1` policy across workspace, path, pinned git, and registry sources. A package name resolves to one effective version and one effective source fingerprint within the active graph.

Pinned git sources are materialized into hermetic mirrors and snapshots under `SPIO_HOME`; transitive path traversal outside a pinned snapshot is rejected. Registry metadata, blobs, and extracted checkout state are cached under `SPIO_HOME/registry/` and verified by immutable digests.

`spio tree` renders the resolver graph directly. `spio check` and `spio lock --check` recompute the active graph and detect lock drift. `spio add`, `spio remove`, `spio fetch`, and `spio sync` operate through the same resolver and canonical write-back pipeline, with rollback when a post-edit resolution fails.

## Alternatives

A formatting-only lock command would be easier to implement but would not prove that the lockfile represents the active dependency graph.

Allowing permissive multi-version resolution early would mimic more mature ecosystems but would complicate lock identity, cache layout, and compile-plan generation before the registry and workflow layers are mature.

Using host-local git working trees directly would be faster in simple cases but would leak host state into supposedly pinned dependency results.

## Consequences

The resolver may reject graphs that a more permissive package manager would accept. That is intentional until the project has stronger version, feature, and registry-policy semantics.

Cache correctness is part of package-manager correctness. Fetching and checking must validate source identity, not only the presence of files.

Dependency-edit commands must remain transaction-like at the manifest and adjacent-lockfile boundary.
