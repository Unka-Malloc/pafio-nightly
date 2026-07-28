# Spio Package Manager Roadmap — Evidence

**Plan:** `package-manager-roadmap`
**Last updated:** 2026-07-11
**Method:** downstream-tree audit (2026-07-11) with file:line citations. Downstream authoritative repo is
`pafio-nightly` (rebranded **spio**; `spio.toml`/`spio.lock`). Upstream `SymPolicy/Pafio` (still Pafio
naming) is reference only; its `package-manager-roadmap/SecurityAudit.md` (AUD-2026-07-001..010) is
mapped to downstream labels in section E.

## A. Per-label findings

| Label | Severity | Finding | Evidence |
|---|---|---|---|
| REQ-SEC-001 | High (P0) | Native fetch verifies only `trust/root.json` SHA-256 pin; no Ed25519 chain | `src/SpioRegistryClient/Client.cpp` ~753–805 vs full verifier `src/spio_registry_v2/validator.py` ~182–404 |
| REQ-SEC-002 | High (P0) | Control-plane `/publish` unauthenticated; accepts arbitrary `archive_path`/`manifest_path` | `src/spio_registry_v2/registry-v2-control-plane-server.py` ~178–210 |
| REQ-SEC-002 | Medium | Control-plane returns HTTP 200 on failure (SPIO-AUD-014) | `docs/audit/EXTERNAL-AUDIT-2026-04-22.md` ~40 |
| REQ-SEC-003 | Medium (P1) | Git snapshot `git archive` + `tar -xf` without prescan (registry path prescans) | `src/SpioResolve/Resolver.cpp` ~316–354 vs `Client.cpp` ~643–707 `ValidateTarListingPaths` |
| REQ-SEC-003 | Medium (P2) | tar `-tf` listing read under 1 MiB stdout cap; truncation may skip members | `src/SpioCore/Process.hpp` ~35–36; `Client.cpp` ~643–707 |
| REQ-SEC-004 | High (P0) | Trust descriptor import stores pins with no signature check; `issued_at`/`expires` not enforced | `src/SpioSecurity/RegistryTrust.cpp` ~196–248 |
| REQ-SEC-005 | Medium (P1) | curl/git/tar spawned with `search_path = true` (PATH hijack) | `src/SpioCore/Process.hpp` ~31; `Client.cpp` ~381 |
| REQ-CON-001 | Medium (P1) | No `flock`/mutex anywhere in `src/`; concurrent processes race SPIO_HOME | repo-wide grep: no lock primitive in `src/` |
| REQ-CON-001 | Medium | TOCTOU on cache existence check-then-act | `src/SpioResolve/Resolver.cpp` EnsureCheckout ~712–721 |
| REQ-ERR-001 | Medium (P1) | Lockfile writes are direct `ofstream` (non-atomic) | `src/SpioApp/PackageApp.cpp` ~286–297; `src/SpioManifest/Dependencies.cpp` ~83–95 |
| REQ-RES-001 | Structural | `single-version-v1` DFS, exact-semver only, self-dep cycle only, thin diamond diagnostics | `src/SpioResolve/Resolver.cpp` ~684–786, ~765–768; `src/SpioManifest/Manifest.cpp` ~131–136 |
| REQ-CACHE-001 | Structural | Git checkouts keyed by FNV-1a64 (not content digest); no re-verify after extraction; no vendor round-trip for registry deps | `src/SpioCore/Paths.cpp`; `Resolver.cpp` ~64–70, ~194; `Client.cpp` ~424–466 |
| REQ-WF-001 | Gap | build/run/test lack machine-readable receipts | `src/SpioApp/WorkflowApp.cpp` |
| REQ-IDE-001 | Gap | `project_graph v1`/`toolchain_state v1` documented but not emitted; Vityo infers from files | `src/SpioResolve/ProjectGraphContract.cpp`; contracts/ schemas |
| REQ-PLAT-001 | Structural | POSIX-only fork/exec; Windows undecided | `src/SpioCore/Process.cpp` ~1–170 |

## B. Trust-boundary analysis (current exposure)

1. **Registry metadata** — a mirror or MITM serving registry objects can forge everything except the
   root pin; the native client consumes index/targets without chain verification (REQ-SEC-001).
2. **Artifacts** — blob SHA-256 is checked after download (`Client.cpp` ~559–615), so a correct-digest
   blob referenced by forged targets is still accepted — artifact integrity depends on metadata trust.
3. **Trust descriptors** — unsigned import means the pin itself is attacker-controllable, which
   undermines REQ-SEC-001 from below (REQ-SEC-004).
4. **Control plane** — unauthenticated publish is a write primitive into the registry and a host-path
   read primitive (REQ-SEC-002).
5. **Git sources** — pinned rev integrity exists, but extraction is unprescanned, so a trusted-but-
   malicious repo escapes the workspace (REQ-SEC-003).
6. **Subprocess PATH** — `search_path=true` lets a local attacker preempt curl/git/tar (REQ-SEC-005).
7. **Concurrency** — shared mutable stores with no locks turn ordinary IDE-plus-shell usage into a
   corruption vector (REQ-CON-001/REQ-ERR-001).

## C. Already-mitigated (do not re-deliver)

| Area | Evidence |
|---|---|
| Prebuilt styio SHA-256 verification | `src/SpioTool/PrebuiltInstall.cpp` ~451–480 |
| Python publisher zip-slip prescan | `src/spio_registry_v2/publisher.py` ~43–54; `tests/.../test_registry_v2.py` |
| Registry blob digest check on fetch | `Client.cpp` ~559–615 |
| Atomic registry download writes (temp+rename) | `Client.cpp` ~101–124, ~367–416 |
| Atomic trust-store write (temp+rename) | `RegistryTrust.cpp` ~91–114 |
| Process wall-clock timeout + SIGKILL, zombie reap | `src/SpioCore/Process.cpp` ~122–131 + waitpid loop (April 2026 audit) |
| OSS-boundary rejection of private auth hooks | `tests/.../SecurityTests.cpp` ~101–122 |

## D. Resolver / cache behavior facts

- **Resolver:** deterministic DFS `SingleVersionResolver::ResolveNode` with fingerprint dedup
  (`Resolver.cpp` ~684–786); same-name divergence → `ResolutionError` (~717–731); self-dep detected via
  fingerprint equality (~765–768); sorted deps/packages/root_ids for determinism (~168–173, ~396–424).
- **Versions:** exact `^\d+\.\d+\.\d+$` only, no ranges/caret/tilde (`Manifest.cpp` ~28–31, ~131–136).
- **Multi-source:** path | git+rev | registry+version, mutually exclusive (`Manifest.cpp` ~174–183).
- **Workspace:** explicit members/exclude, no globs (`Resolver.cpp` ~494–540).
- **Cache layout:** content-addressed registry blobs `registry/blobs/sha256/xx/yy/<digest>.tar`; FNV-keyed
  index and git repo dirs; extracted checkouts under `registry/checkouts/...` (`Paths.cpp`).
- **Offline:** `ResolveOptions.offline` requires cache/vendor satisfaction (`Resolver.cpp` ~209–221).

## E. Upstream SecurityAudit → downstream label mapping

| Upstream AUD (SecurityAudit.md) | Downstream label |
|---|---|
| AUD-2026-07-001 native TUF chain | REQ-SEC-001 |
| AUD-2026-07 control-plane auth | REQ-SEC-002 |
| AUD trust descriptor crypto | REQ-SEC-004 |
| AUD archive prescan parity | REQ-SEC-003 |
| AUD subprocess PATH | REQ-SEC-005 |
| AUD timeout policy | mitigated (section C) |
| Upstream REQ-CACHE-001/002 | REQ-CACHE-001 |
| Upstream REQ-WF-001/002 | REQ-WF-001 |
| Upstream REQ-IDE-001 | REQ-IDE-001 |
| Upstream REQ-WS-001 | folded into REQ-RES-001 (workspace member handling) |
| (downstream-added) concurrency/atomicity | REQ-CON-001, REQ-ERR-001 |
| (downstream-added) platform matrix | REQ-PLAT-001 |

Naming note: upstream fixtures use `pafio.toml`/`pafio.lock` and `Styio-for-Pafio-Developers.md`;
downstream is fully rebranded to `spio.*` and `Styio-for-Spio-Developers.md`. April 2026 audit fixes
(control-plane redaction, tar canonical paths, process timeouts) are present in both trees.

## F. Priority ordering justification

1. P0 security first: REQ-SEC-001 (TUF) and REQ-SEC-004 (descriptor signing) are interlocked — the
   descriptor is the chain's bootstrap — followed by REQ-SEC-002 (control plane, the publish root).
2. REQ-SEC-003 and REQ-SEC-005 are independent P1 hardening, parallelizable after architecture sign-off.
3. REQ-CON-001/REQ-ERR-001 are a correctness floor for IDE-driven concurrent use.
4. REQ-RES-001/REQ-CACHE-001 close the contract debt the trust story depends on (cache = CAS).
5. REQ-WF-001/REQ-IDE-001/REQ-PLAT-001 close the supplier side of Vityo integration and the platform
   decision. Final validation runs the adversarial suite last.
