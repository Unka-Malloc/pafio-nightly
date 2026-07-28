# Spio Package Manager Roadmap — Architecture

**Plan:** `package-manager-roadmap`
**Last updated:** 2026-07-11
**Nature:** brownfield record. The native tree already layers cleanly; this plan adds shared
security/concurrency primitives that multiple fix nodes consume, so the primitives must be designed once
here before the nodes touch code.

## 1. Module / target map (current)

```
spio (executable)
  └── spio_cli_shell            CLI.cpp routing
      └── spio_cli_commands     SpioApp/* orchestration
          ├── spio_project_service   ProjectGraphContract
          ├── spio_package_service   Workflow, Vendor, Pack, Publish, Tree
          ├── spio_toolchain_service Tool, Toolchain, Cloud, Plan
          ├── spio_resolution        Resolver, RegistryClient, Security
          └── spio_manifest          Manifest, Lockfile
              └── spio_foundation    Core, Compat
```

Python side: `src/spio_registry_v2/{publisher,validator,keygen,common}.py` (full TUF, authoritative for
publisher-side checks). All subprocesses go through `src/SpioCore/Process.*` only.

## 2. Trust-boundary placement (which module enforces what)

| Guarantee | Owning module | Notes |
|---|---|---|
| TUF chain verification (native) | **SpioSecurity** (new `TufVerifier`) | ports `validator.py` order; consumed by `SpioRegistryClient` |
| Trust descriptor signature + expiry | **SpioSecurity** (`RegistryTrust`) | shares Ed25519 primitive with `TufVerifier` |
| Archive prescan (all paths) | **SpioSecurity** (`PrescanArchive`, moved from registry client) | consumed by both `SpioRegistryClient` and `SpioResolve` |
| Subprocess hardening | **SpioCore/Process** | absolute tool paths, `search_path=false` |
| Inter-process locking | **SpioCore** (`FileLock`) | per-store scope |
| Atomic state writes | **SpioCore** (`AtomicFile`) | temp+fsync+rename |
| Content digests / CAS | **SpioCore/Paths** + **SpioRegistryClient**/`SpioResolve` cache paths | digest as identity |

## 3. New shared component contracts

| Component | Interface (conceptual) | Consumers |
|---|---|---|
| `TufVerifier` | `verify_chain(metadata_set, trusted_root) -> VerifiedTargets \| Error` (pure: bytes → verdict) | RegistryClient metadata consumption |
| Ed25519 verify | `verify(pubkey, msg, sig) -> bool` via OpenSSL `pkeyutl -verify` (matches Python `common.py`; absolute `SPIO_OPENSSL` / PATH resolve, `search_path=false`) | TufVerifier, RegistryTrust |
| `RegistryTrust` (extended) | `import_descriptor(bytes) -> PinSet \| Error` with signature+expiry enforcement | trust bootstrap |
| `PrescanArchive` | `prescan(listing_lines, extract_root) -> ConfinedPaths \| Error` (reject abs/drive/`..`/escaping symlink; fail closed on overflow) | RegistryClient, Resolver (git) |
| `FileLock` | `acquire(store_path, scope, timeout) -> Guard \| LockHeldError` (advisory, POSIX flock now) | cache/trust/lockfile mutations |
| `AtomicFile` | `write(path, bytes)` = same-dir temp + fsync + rename | lockfiles, cache indexes, trust store |

Lock ordering (deadlock-free): **project lockfile → cache store → trust store**. Reads of the immutable
content-addressed store take no lock.

## 4. Dependency direction

Services (`spio_resolution`, `spio_package_service`, `spio_project_service`, `spio_toolchain_service`)
depend on **SpioSecurity** and **SpioCore**; those two never depend back up. `spio_manifest` stays below
services; `spio_foundation` remains the base. No new upward edges. The Python registry tooling stays a
sibling surface consumed only through file/CLI contracts, never linked.

## 5. Per-node file ownership (disjoint where parallel, sequenced where shared)

| Node (label) | Owns (writes) | Shared-file sequencing |
|---|---|---|
| TUF (REQ-SEC-001) | `SpioSecurity/TufVerifier.*`, `Client.cpp` metadata consumption | `Client.cpp` also touched by cache node → TUF first |
| Control plane (REQ-SEC-002) | `registry-v2-control-plane-server.py`, control-plane contracts, interop test | disjoint |
| Prescan (REQ-SEC-003) | `SpioSecurity/PrescanArchive.*`, `Resolver.cpp` git extraction, `Process.hpp` listing cap | `Resolver.cpp` also touched by resolver+cache nodes → prescan first |
| Descriptors+subprocess (REQ-SEC-004/005) | `RegistryTrust.cpp`, `Process.hpp` spawn defaults, curl/git/tar call sites | shares Ed25519 with TUF (contract in §3) |
| Locking+atomic (REQ-CON-001/ERR-001) | `SpioCore/FileLock.*`, `SpioCore/AtomicFile.*`, lockfile write sites, `EnsureCheckout` | AtomicFile adopted by cache node afterward |
| Resolver (REQ-RES-001) | `Resolver.cpp` resolution logic, `Manifest.cpp` version policy, conventions doc | after prescan node (same `Resolver.cpp`) |
| Cache (REQ-CACHE-001) | `Paths.cpp`, cache read/write in `Client.cpp`+`Resolver.cpp`, `Vendor.cpp` | after TUF + prescan + locking (shared files) |
| Workflow/IDE/platform (REQ-WF/IDE/PLAT) | `WorkflowApp.cpp`, `ProjectGraphContract.cpp`, contracts, docs, mirror fixtures | disjoint |

Sequencing summary: on `Client.cpp` — TUF → cache; on `Resolver.cpp` — prescan → resolver → cache;
Ed25519 primitive lands with TUF and is reused by descriptors. All other nodes are pairwise disjoint and
parallelizable after this architecture node.

## 6. Design-pattern decisions

| Decision | Rationale |
|---|---|
| Pure verifier (bytes → verdict) for TUF/prescan | unit-testable without network/filesystem; parity with Python fixtures |
| One shared prescan/one shared crypto primitive | eliminates the acquisition-path asymmetry the audit flagged; single review surface |
| Advisory per-store locks, not one global lock | preserves parallelism on independent stores; documented order prevents deadlock |
| Temp+fsync+rename everywhere (AtomicFile) | matches the pattern already proven in `Client.cpp`; crash-safe |
| Digest-as-identity CAS | makes the cache trustworthy under the TUF story; FNV stays a directory-name shortcut only |
| **Pattern-free by choice:** CLI dispatch, receipt serialization | already single-purpose; no indirection added |

## 7. Scaffold status

No scaffold code is created here. `SpioSecurity` and `SpioCore` exist; new files (`TufVerifier`,
`PrescanArchive`, `FileLock`, `AtomicFile`) are created by their owning nodes together with tests, so no
empty placeholders are introduced. Windows `FileLock`/`Process` mapping is deferred to the REQ-PLAT-001
decision node rather than stubbed now.
