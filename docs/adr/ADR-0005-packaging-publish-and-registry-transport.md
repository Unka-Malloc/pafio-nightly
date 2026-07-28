# ADR-0005: Packaging, Publish, and Registry Transport

**Purpose:** Record the functional decision that deterministic packaging, publish preflight, local registry transport, remote registry transport, and static registry layout share one package distribution contract.

**Last updated:** 2026-06-28

## Status

Accepted.

## Supersedes

- `ADR-0015-phase4-pack-deterministic-source-archive.md`
- `ADR-0016-phase6-publish-dry-run-preflight-without-registry-transport.md`
- `ADR-0021-phase6-filesystem-registry-publish-transport.md`
- `ADR-0023-phase6-url-based-registry-consume-and-cloud-static-layout.md`
- `ADR-0024-phase6-anonymous-http-remote-registry-publish.md`
- `ADR-0026-split-registry-origins-use-promotion-into-read-root.md`
- `ADR-0027-remote-publish-reserves-write-origin-header-hooks.md`
- `ADR-0028-remote-publish-policy-file-for-write-origin-hooks.md`
- `ADR-0029-remote-publish-profile-discovers-policy-under-spio-home.md`

## Context

Publishing is not useful until source archives, package metadata, dependency metadata, registry indexes, and client downloads all describe the same immutable package identity.

The project now has deterministic local packaging, local publish preflight, filesystem registry publishing, URL-based registry consumption, and remote HTTP publishing over the same static layout.

## Decision

`spio pack` creates deterministic source archives from the selected package. `spio publish --dry-run` reuses the same package archive and metadata validation path without writing to a registry.

The registry layout is static and cloud-deployable. Immutable archive blobs, package metadata, version index entries, and dependency metadata are shared by local filesystem registries and URL-addressable `file://`, `http://`, and `https://` read roots.

The first write transports are local filesystem publication and anonymous remote HTTP `PUT` publication. Remote writes keep the same blob-and-index layout as local publication.

Publication and consumption are operationally separate. Write origins may upload into a writable root and then promote immutable content into a read-only serving root.

Remote publish supports write-origin integration hooks through request headers, reusable TOML policy files, and named policy profiles discovered under `SPIO_HOME`.

Path and git dependencies are not valid in publishable package artifacts unless they have been converted into registry-addressable dependencies.

## Alternatives

Starting with mutable git-based publish semantics would be familiar but would not establish immutable package identity.

Adding remote registry auth before the static layout and publish preflight were stable would mix security policy with basic distribution semantics.

Using different layouts for local and remote registries would reduce reuse and make cloud publication a separate product instead of an extension of the package repository contract.

## Consequences

Package archives, registry entries, and lockfile registry sources must agree on package name, version, registry root, and digest.

Remote writes remain intentionally minimal until the security boundary and trust policy are stronger.

Write-origin hooks are integration hooks only; they do not turn the public open-source tree into the owner of private auth or deployment policy.
