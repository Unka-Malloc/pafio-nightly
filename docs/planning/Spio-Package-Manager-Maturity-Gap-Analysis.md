# Spio Package Manager Maturity Gap Analysis

**Purpose:** Rank the current gaps between `spio` and mature package-manager behavior, from highest to lowest priority, so implementation work can close capability gaps without overstating the current product surface.

**Last updated:** 2026-05-19

## Current Baseline

`spio` is no longer a package-manager scaffold. The current tree has real manifest and lockfile parsing, canonical write-back, workspace/path/git/registry resolution, `single-version-v1` locking, dependency editing, `sync`, `fetch`, `tree`, `vendor`, deterministic source packaging, registry publish and fetch paths, registry trust descriptor import, managed `styio` installation and switching, project-local toolchain pins, and dry-run or live workflow handoff through the `styio` compile-plan boundary.

The remaining gaps are therefore not basic command names. They are the maturity layers that make a package manager safe, ergonomic, reproducible, and operable at ecosystem scale.

This document uses mature package managers such as Cargo, Go modules, npm, pnpm, uv, and rustup as reference points. It does not require `spio` to copy their exact user experience; it uses them to identify expected capability classes.

## Priority Order

### P0. End-To-End Workflow Reliability

These gaps block `spio` from feeling like the default developer workflow.

| Rank | Gap | Current state | Mature package-manager expectation | Closure target |
|------|-----|---------------|------------------------------------|----------------|
| P0-1 | Release-hardened `build`, `run`, and `test` execution | Compile-plan v1 handoff exists, but release hardening and matrix coverage are still ongoing. | Normal build/test/run commands work without wrapper scripts and produce inspectable artifacts, diagnostics, and repeatable failure modes. | Published `styio` binaries pass compile-plan gates across supported targets; `spio` writes stable receipts and captures compiler output roots for every non-dry-run workflow. |
| P0-2 | Fast local feedback loop | Workflow commands are functional, but watch mode and incremental cache reuse are not a first-class developer path. | Common commands provide fast repeated execution through incremental rebuilds, watch mode, and stable cache keys. | Add `spio build --watch`, `spio test --watch`, and artifact reuse keyed by graph, toolchain, target, profile, and source digest. |
| P0-3 | Black-box integration coverage | Native fixtures are strong, but the open defect record still calls out placeholder or optional integration coverage. | Core install, resolve, lock, offline, registry, failure, rollback, and CI paths are proven by runnable black-box suites. | Replace placeholder integration runners with real package-manager scenarios and make them part of CTest and CI. |
| P0-4 | CI parity with release gates | CI can skip some audit or health checks depending on profile. | The PR path and release/checkpoint path prove the same package-manager invariants or explicitly document equivalent coverage. | Align CI, submit, delivery, health, and external audit checks so a green PR cannot bypass required package-manager gates. |
| P0-5 | Supported release artifact matrix | Alpha docs defer some platform artifacts and self-contained archives. | Fresh-machine installation works on every advertised platform, with verified binaries and clear upgrade/removal semantics. | Publish real `spio` and `styio` prebuilts for the advertised matrix, including x86_64 Linux when claimed, and verify `spio doctor` on each target. |

### P1. Registry And Supply-Chain Trust

These gaps block public or multi-publisher registry operation.

| Rank | Gap | Current state | Mature package-manager expectation | Closure target |
|------|-----|---------------|------------------------------------|----------------|
| P1-1 | Hosted authenticated publish service | A local HTTP control-plane reference and local v2 worker exist; hosted tenancy and auth policy are future work. | Registry publication requires authenticated publishers, namespace authorization, scoped tokens, rate limits, and audit logs. | Implement or integrate the hosted publish control plane with publisher identity, namespace ownership, token scopes, and append-only audit logs. |
| P1-2 | Namespace and ownership governance | Namespace policy is documented as a production requirement, but not a shipped ecosystem service. | Packages have owners, maintainers, delegation, transfer, recovery, and dispute workflows. | Add namespace ownership records, owner management commands or service APIs, and gates that reject unauthorized publish/yank/deprecate actions. |
| P1-3 | Full registry trust policy | Current clients pin `trust/root.json` through imported descriptors and verify source artifact digests; full signed timestamp/snapshot/targets parity remains residual work. | Clients verify signatures, thresholds, expiry, monotonic versions, snapshot digests, revocation, and replay protection. | Enforce timestamp, snapshot, targets, threshold, expiry, monotonic-version, and revocation rules in the native client and test malformed metadata. |
| P1-4 | Key rotation and revocation | Role keys and signed metadata exist, but production rotation workflows are not complete. | Registries can rotate compromised or expired keys without breaking safe clients. | Define root rotation, targets delegation, key revocation, recovery, and client update behavior with positive and negative tests. |
| P1-5 | Transparency log maturity | The registry uses a signed hash-chain checkpoint, not a full public transparency system. | Clients can detect equivocation and replay using persisted checkpoints or a verifiable transparency structure. | Add persisted client checkpoints, replay detection, checkpoint consistency checks, and a future Merkle tile or equivalent scalable log format. |
| P1-6 | Package provenance and attestation | SHA-256 verification exists; signed provenance is deferred. | Packages can prove builder identity, source revision, build recipe, and artifact custody. | Emit and verify source provenance, build provenance, signing identity, and release pipeline evidence for published artifacts. |
| P1-7 | Vulnerability and advisory workflow | No first-class advisory database, `spio audit`, or dependency vulnerability check is present. | Users can audit lockfiles against advisories and receive actionable remediation data. | Add advisory schema, registry or mirror distribution, lockfile audit command, severity policy, ignore/expiry rules, and CI integration. |
| P1-8 | License and policy enforcement | General dependency usage docs exist, but package resolution does not enforce license or policy decisions. | Organizations can block packages by license, source, provenance, age, or registry trust tier. | Add policy files, package metadata fields, resolver/publish checks, and machine-readable policy violations. |
| P1-9 | Secure credential storage and enterprise network config | Public builds reserve auth hooks but do not ship credential storage or corporate proxy/cert policy. | Credentials are stored outside manifests, support keychains or env-backed providers, and respect proxies and custom CAs. | Add credential provider boundaries, secure storage guidance, proxy/cert configuration, redaction tests, and auth-specific private gates. |
| P1-10 | Yanking, deprecation, and state transitions | The v2 protocol reserves yank/deprecate actions, but user-facing lifecycle commands and hosted behavior are not complete. | Maintainers can yank, deprecate, undeprecate, and explain release status without mutating artifacts. | Add publish-control-plane actions, read-side state interpretation, CLI UX, lock behavior, and tests for yanked/deprecated releases. |

### P2. Resolver, Lockfile, And Offline Maturity

These gaps separate the current conservative resolver from mature ecosystem dependency management.

| Rank | Gap | Current state | Mature package-manager expectation | Closure target |
|------|-----|---------------|------------------------------------|----------------|
| P2-1 | Semver range solving | Registry dependencies require exact `version = "x.y.z"` and `single-version-v1` rejects competing versions. | Users can express compatible ranges, prerelease policy, minimum versions, and update constraints while retaining deterministic locks. | Add a range syntax, solver policy, prerelease rules, conflict diagnostics, and lockfile recording of selected versions. |
| P2-2 | Selective update and upgrade commands | `sync` refreshes the active graph; there is no mature `update`, `upgrade`, or minimal-change resolver UX. | Users can update one package, update transitive dependencies, apply conservative or eager strategies, and inspect changes. | Add `spio update`, selective lock refresh, lock diff summaries, and solver strategy flags. |
| P2-3 | Conflict explanation | Resolution errors fail closed, but mature "why this version" and conflict traces are not complete. | Users can ask why a package/version is present and why a constraint failed. | Add `spio tree --why`, conflict traces, selected-version explanations, and JSON diagnostics for solver failures. |
| P2-4 | Feature flags and optional dependencies | Published records reserve feature declarations, but manifest, resolver, and build workflows do not yet support mature feature selection. | Users can enable features, optional deps, default features, and target-specific dependency sets reproducibly. | Add manifest syntax, feature unification policy, lockfile recording, publish metadata, and build-plan propagation. |
| P2-5 | Target/platform conditional dependencies | Current manifests separate runtime and dev dependencies, but not target-conditional or platform-specific dependency graphs. | Packages can declare dependencies for OS, architecture, ABI, profile, and toolchain conditions. | Add target condition syntax, resolver evaluation, lock representation, and package metadata validation. |
| P2-6 | Registry vendoring | `spio vendor` focuses on pinned git snapshots; registry-era vendoring remains incomplete. | Offline builds can commit or mirror exact registry sources and metadata with verification. | Vendor registry metadata, blobs, trust pins, and extracted snapshots; add `vendor --check` and offline CI gates. |
| P2-7 | Exact sync and environment pruning | `sync` prepares dependencies, but there is no full "make local state exactly match lock" cleanup mode. | Package managers can remove unused cached or project-local state and prove the environment matches the lock. | Add `spio sync --exact` or equivalent, stale checkout detection, and safe cleanup of unused project-local materialization. |
| P2-8 | Cache garbage collection and repair | Manual cleanup is documented; no full cache prune/verify/repair command exists. | Users can inspect, verify, prune, and repair caches without deleting all state. | Add `spio cache status`, `spio cache verify`, `spio cache prune`, and corruption recovery for git, registry, tool, and build caches. |
| P2-9 | Content-addressed source and artifact store | Registry blobs are digest-keyed, but the broader source/build cache model is not fully content-addressed. | Shared cache stores dedupe immutable sources and artifacts by digest with reproducible cache keys. | Move source archives, normalized trees, and build artifacts behind content-addressed keys and preserve project-local active outputs separately. |
| P2-10 | Lockfile portability and compatibility policy | Lockfile v1 is stable, but custom lock locations, multi-lock workspaces, and lock migration policy are narrow. | Lockfiles have explicit compatibility, migration, and workspace behavior across tool versions. | Define lockfile migration rules, forward/backward compatibility tests, optional workspace-level lock policies, and lockfile explanation tools. |

### P3. Workspace, Toolchain, And Build Ergonomics

These gaps matter once the package manager is used in larger repositories.

| Rank | Gap | Current state | Mature package-manager expectation | Closure target |
|------|-----|---------------|------------------------------------|----------------|
| P3-1 | Workspace filters and default members | Ambiguous multi-package operations require explicit package selection; graph-aware filters are not complete. | Large workspaces can target all, default members, changed packages, dependents, dependencies, or named subsets. | Add workspace default members, `--workspace`, `--exclude`, graph-aware `--filter`, changed-package selection, and affected-package test gates. |
| P3-2 | Patch, replace, and override model | Path and workspace deps exist, but there is no root-level override model for temporary local development. | Workspaces can override registry deps with local paths or alternate sources without publishing those overrides. | Add `[patch]` or `[replace]`-style workspace root syntax, publish exclusion rules, and lockfile provenance. |
| P3-3 | Workspace-only dependency intent | Local workspace resolution is inferred from source kind and package identity. | A manifest can require that a dependency resolve only from the workspace. | Add explicit workspace dependency syntax and resolver errors when a required local dependency resolves from registry/git/path instead. |
| P3-4 | Build profiles beyond current minimal/dev/release scope | `minimal`, `dev`, and `release` vocabulary exists, but profile configuration is not a mature user-controlled build model. | Profiles can control optimization, debug info, target directories, test behavior, and dependency build settings. | Add profile schema, inheritance/overrides, target-specific profile fields, and compile-plan propagation. |
| P3-5 | Toolchain component and target management | Managed `styio` install/use/pin exists, but components, target triples, and policy-driven auto-install are not mature. | Toolchain managers install components/targets, honor project minimum versions, and explain missing toolchain requirements. | Add toolchain target/component metadata, project minimum compiler constraints, optional auto-install policy, and clear failure UX. |
| P3-6 | Binary artifact selection | Registry v2 reserves binary artifact paths, but source artifacts remain the implemented authority. | Package managers can choose trusted binary artifacts by target, ABI, profile, and feature fingerprint with source fallback. | Add binary publish pipeline, lockfile binary artifact fields, selection policy, fallback rules, and verification. |
| P3-7 | Multi-registry and source replacement policy | Manifests name explicit registry URLs; global registry aliases and source replacement are not mature. | Users can configure named registries, mirrors, source replacement, and corporate policy without editing every manifest. | Add registry alias config, mirror/source replacement rules, lockfile recording of resolved roots, and policy gates. |
| P3-8 | Project configuration layering | Some project-local toolchain state exists, but general config precedence is narrow. | Config has clear precedence across CLI, env, project, user, workspace, and system scopes. | Define and implement `spio config` or equivalent for registries, network, cache, toolchain, policy, and output settings. |
| P3-9 | Command discovery and shell UX | Core help exists, but completions, manpages, and richer command discovery are not complete. | Users get shell completions, command aliases where appropriate, stable help examples, and discoverable subcommands. | Add completion generation, manpage or reference generation, and docs gates that keep examples executable. |
| P3-10 | Package metadata richness | Package identity and dependency metadata exist, but mature metadata fields are limited. | Packages expose description, license, repository, homepage, keywords, categories, readme, authors, links, and documentation URLs. | Extend manifest, publish preflight, registry records, and package display commands with metadata validation. |

### P4. Ecosystem Services And Long-Term Operations

These gaps are lower priority for current implementation but expected in a mature ecosystem.

| Rank | Gap | Current state | Mature package-manager expectation | Closure target |
|------|-----|---------------|------------------------------------|----------------|
| P4-1 | Search, package info, and discovery | Registry roots are explicit; no public search/index service or `spio search/info` UX exists. | Users can find packages, inspect versions, read metadata, and compare release states. | Add searchable metadata index, `spio search`, `spio info`, and registry-side package pages or machine endpoints. |
| P4-2 | Hosted user/account experience | Hosted tenancy is future work. | Users can create accounts, manage tokens, rotate keys, invite maintainers, and recover ownership. | Implement account APIs and operational flows outside the static read plane. |
| P4-3 | Operational metrics and observability | Local gates exist; production registry/service metrics are not a complete package-manager surface. | Operators see publish/fetch latency, error rates, cache hit rates, abuse signals, and audit trails. | Add metrics, structured logs, dashboards, and incident runbooks for registry and hosted control planes. |
| P4-4 | Mirror, proxy, and CDN freshness tooling | Split-origin promotion exists, but generic mirror health and CDN freshness UX are limited. | Operators can verify mirrors, detect stale replicas, and control proxy fallback behavior. | Add mirror verification, freshness reports, promotion status, client stale-metadata diagnostics, and mirror failover config. |
| P4-5 | Migration and deprecation tooling | Docs record compatibility constraints, but user-facing migration tooling is limited. | Users can migrate manifests, locks, config, and registry metadata across versions with clear warnings. | Add `spio fix` or migration commands, compatibility checks, and structured deprecation reports. |
| P4-6 | Plugin or hook system | No general plugin/lifecycle script system is present, which is a deliberate safety-positive omission for now. | Mature ecosystems often provide hooks, but safe package managers constrain them carefully. | Decide whether hooks belong in `spio`; if yes, design sandboxing, explicit opt-in, reproducibility rules, and CI policy before implementation. |
| P4-7 | Hosted web UI and documentation publishing | Registry docs describe service boundaries, not a package portal. | Public ecosystems expose package pages, docs links, download stats, ownership, advisories, and changelogs. | Build package portal endpoints only after registry identity, metadata, and advisories are stable. |
| P4-8 | Cross-language or foreign-package interoperability | `spio` is focused on Styio packages and compiler handoff. | Some mature managers interoperate with external ecosystems or vendored native dependencies. | Defer until Styio package identity, source artifacts, and build profiles are stable. |

## Non-Gaps

The following are already materially present and should not be counted as missing without checking current implementation:

1. Manifest and lockfile parsing, validation, and canonical write-back.
2. Workspace, path, pinned git, and registry dependency source kinds.
3. Resolver-backed `lock`, `fetch`, `sync`, `tree`, `add`, and `remove`.
4. `--locked`, `--offline`, `--frozen`, and project-local vendoring for pinned git snapshots.
5. Deterministic source package archive creation.
6. Local filesystem registry publication.
7. Remote HTTP registry publication against the current control-plane boundary.
8. Static registry consumption through `file://`, `http://`, and `https://` roots.
9. Registry trust descriptor import and root digest pinning for remote fetches.
10. Managed `styio` install, update, list, uninstall, use, status, and project pinning.
11. Project graph introspection.
12. Local cloud execution-policy and build-job-request contract generation.

## Recommended Near-Term Cut

The highest-value next implementation slice should close these in order:

1. P0-3 and P0-4: turn the open test-framework defect into real black-box package-manager gates and CI parity.
2. P1-3 and P1-4: finish native registry trust-chain parity and key-rotation behavior before broader public registry use.
3. P2-6 and P2-8: make offline and cache operations operationally inspectable instead of relying on manual cleanup.
4. P2-1 and P2-2: add semver ranges and selective update only after the trust and offline layers are harder.
5. P3-1 and P3-2: improve large-workspace ergonomics after the resolver has stronger update and override semantics.

This ordering keeps the project aligned with its existing direction: deterministic state first, trust and verification before convenience, and explicit workspace behavior before broad magic.
