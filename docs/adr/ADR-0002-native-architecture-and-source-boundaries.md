# ADR-0002: Native Architecture and Source Boundaries

**Purpose:** Record the functional architecture decision that `spio` is a native C++20 and CMake package manager with explicit source-tree and target-graph boundaries.

**Last updated:** 2026-06-28

## Status

Accepted.

## Supersedes

- `ADR-0002-native-cpp20-cmake-phase2-core.md`
- `ADR-0025-registry-client-server-source-and-docs-split.md`
- `ADR-0031-native-target-graph-split.md`

## Context

The project began with bootstrap scaffolding, but the package-manager core now needs one authoritative implementation path. It also contains several distinct responsibilities: CLI parsing, manifest and lockfile handling, dependency resolution, registry consumption, registry publication, compile-plan generation, toolchain management, and documentation gates.

Keeping all behavior behind one monolithic native target would make it harder to enforce ownership boundaries and harder to test package-manager domains independently.

## Decision

The authoritative implementation path is native C++20 built with CMake.

The CLI shell stays thin. Package-manager behavior is split across native backend domains that match the responsibility boundaries exposed in governance documents: manifest/lock, resolver, registry client, registry server or publish transport, workflow and toolchain state, and command rendering.

Registry consumption and registry publication remain in one repository because they share package metadata, archive, and index contracts. They are separated at source and documentation boundaries so client reads do not accidentally inherit server-write assumptions.

Public command contracts and external process boundaries remain stable even when internal target boundaries change.

## Alternatives

Continuing a bootstrap-language implementation in parallel would have kept early iteration speed but would create two authoritative behavior paths.

Keeping a single native core target would be simpler in the short term but would let domain coupling accumulate around the CLI.

Splitting registry client and server concerns into separate repositories would reduce local coupling but would slow changes to the shared package and index contract.

## Consequences

Native tests should be organized around backend domains, not only around CLI snapshots.

The CLI may orchestrate domains but should not become the owner of resolver, registry, toolchain, or documentation lifecycle rules.

Future extraction of registry server code remains possible because the source and documentation split already exists, but extraction is not required for the current package-manager maturity level.
