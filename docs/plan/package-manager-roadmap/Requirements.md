# Spio Package Manager Roadmap — Requirements

**Plan:** `package-manager-roadmap`
**Last updated:** 2026-07-11

## User Problem

spio is the local-first package manager and project workflow client for Styio, but its trust story is
asymmetric and its state handling is unsafe under concurrency: the Python publisher/validator implements
full TUF verification while the native client trusts registry metadata after a single root-pin check;
the publish control plane accepts unauthenticated requests with arbitrary host paths; git-sourced
archives extract without traversal prescan; two concurrent `spio` processes can tear `spio.lock` or race
the cache; and the resolver/cache contracts (exact versions, FNV cache identity, cycle behavior) are
folklore rather than documented, tested contracts. Vityo additionally waits on machine payloads spio
documents but does not emit.

## Target Users and Workflows

- **Styio project developers** — `spio new/add/sync/build/run/test` daily loop, offline-capable.
- **Package publishers** — `spio pack/publish` against the registry v2 control plane.
- **Registry operators** — trust descriptor distribution, key rotation, control-plane deployment.
- **Vityo IDE** — consumes `project_graph v1` / `toolchain_state v1` payloads and workflow receipts.
- **CI operators** — deterministic resolution and receipts in pipelines.

## Trust Boundaries (what each requirement defends)

| Boundary | Attacker capability today | Defending requirement |
|---|---|---|
| Registry metadata/artifacts | serve forged snapshot/timestamp/targets past the pin-only native check | REQ-SEC-001 |
| Publish control plane | unauthenticated publish; read/publish arbitrary host paths | REQ-SEC-002 |
| Acquired archives (git path) | zip-slip/symlink escape into workspace | REQ-SEC-003 |
| Trust descriptor files | pin a malicious registry root via handed descriptor | REQ-SEC-004 |
| Subprocess PATH | PATH-planted curl/git/tar executes | REQ-SEC-005 |
| Shared SPIO_HOME / project state | concurrent processes corrupt cache/lockfiles | REQ-CON-001 / REQ-ERR-001 |

## Functional Requirements

- **REQ-SEC-001 — Native TUF chain parity.**
  The native registry client verifies the complete signed chain (root → timestamp → snapshot → targets;
  Ed25519 thresholds, version monotonicity, expiry, hash links) before trusting any registry object,
  with verdict parity against the Python validator on shared fixtures.
  *Acceptance target:* tampering any role file fails `MaterializeRegistryPackage` with a stable error and
  no cache write; parity fixtures agree; offline mode works from previously verified cache.

- **REQ-SEC-002 — Authenticated, confined control plane.**
  Mutating control-plane endpoints require bearer auth (deny-by-default when unconfigured); publish
  inputs resolve strictly inside a staging directory; failures return correct HTTP codes with structured
  errors (closes SPIO-AUD-014).
  *Acceptance target:* auth-negative and traversal-negative interop tests pass.

- **REQ-SEC-003 — Universal archive prescan.**
  One shared prescan routine validates entry paths and symlink targets for **all** acquisition paths
  (registry and git), with tar listings read without the 1 MiB truncation window (fail closed on
  overflow).
  *Acceptance target:* zip-slip/symlink fixtures fail on the git path; oversized-listing fixture fully
  scanned or failed closed; both paths call the shared routine.

- **REQ-SEC-004 — Signed, expiring trust descriptors.**
  Descriptor import verifies an Ed25519 signature over canonical bytes against configured signer keys and
  enforces `issued_at`/`expires` with skew tolerance; unsigned/expired/unknown-signer descriptors are
  rejected; dev-trust requires an explicit loud flag.
  *Acceptance target:* SecurityTests negative cases pass.

- **REQ-SEC-005 — Absolute-path subprocess invocation.**
  curl/git/tar resolve to absolute paths at startup (env/config override per existing `SPIO_*` contract);
  security-relevant spawns set `search_path = false`.
  *Acceptance target:* PATH-hijack simulation fails to hijack.

- **REQ-CON-001 — Inter-process store locking.**
  Per-store advisory locks (project lockfile, cache store, trust store; documented order
  project → cache → trust) guard mutation windows with bounded wait and clear lock-held diagnostics;
  reads of immutable content-addressed objects stay lock-free.
  *Acceptance target:* two parallel `spio sync` processes finish with one winner or a clean lock error
  and byte-consistent stores.

- **REQ-ERR-001 — Atomic state writes and failure recovery.**
  Every mutable state file (`spio.lock`, `spio-toolchain.lock`, cache indexes) is written via
  same-directory temp + fsync + rename; crash-mid-write leaves no torn state; error taxonomy keeps
  domain exceptions, ranged exit codes, and stderr snippets intact.
  *Acceptance target:* power-fail simulation in LockTests proves no torn lockfile.

- **REQ-RES-001 — Resolver contract closure.**
  The exact-version `single-version-v1` policy is decided and documented (keep-with-rationale or bounded
  range extension via a requirements update); full cycle detection names the cycle path; diamond
  conflicts print both requirement chains; resolution stays deterministic.
  *Acceptance target:* cycle/diamond/golden-lockfile test vectors pass; policy recorded in the
  conventions doc.

- **REQ-CACHE-001 — Verified content-addressed store and offline vendoring.**
  Every cached artifact class carries a content digest (git snapshots included; FNV keys stop being
  identity); reads verify lazily with a verified-marker; corruption self-heals online and fails
  precisely offline; vendor export/import round-trips registry deps for fully-offline sync.
  *Acceptance target:* corruption, offline, and vendor round-trip tests pass.

- **REQ-WF-001 — Workflow receipts.**
  `spio build/run/test` emit schema-versioned JSON receipts (phases, exit status, artifacts, timings)
  alongside human output.
  *Acceptance target:* receipts validate against `contracts/` schemas in tests.

- **REQ-IDE-001 — Vityo machine payloads.**
  spio emits `project_graph v1` and `toolchain_state v1` payloads with source-confidence fields; mirror
  fixtures consumed by vityo-nightly gates are updated.
  *Acceptance target:* schema validation plus cross-repo mirror gate green.

- **REQ-PLAT-001 — Platform matrix decision.**
  The POSIX-only process layer's Windows scope is decided and recorded (deferred-with-rationale or a
  Process port plan), consistent with REQ-CON-001's locking design.
  *Acceptance target:* decision recorded in governance/build docs.

## Non-Functional Constraints

1. **Frozen CLI contract** (`docs/governance/Spio-CLI-Contract.md`): new surfaces are additive; existing
   command forms, flags, and exit-code ranges do not drift.
2. **Local-first independence** (README.md): no styio implementation linkage; platform connection stays
   optional; offline flows are first-class.
3. **OSS boundary:** private auth hooks stay out of the open tree (SecurityTests enforce); the bearer
   scheme is the documented open mechanism.
4. **Determinism:** resolution and lockfile serialization remain byte-deterministic.
5. **Performance:** `scripts/perf-gate.py` against `tests/perf/baseline.json` stays green (15%
   threshold); verification costs must fit it.
6. **Governance SSOT:** protocol/layout changes update `docs/registry/` and `docs/governance/` in the
   same commit as the code.

## Scope

`pafio-nightly` repository: `src/` native libraries (SpioSecurity, SpioCore, SpioRegistryClient,
SpioResolve, SpioManifest, SpioApp, SpioVendor), `src/spio_registry_v2/` Python tooling, `contracts/`,
`tests/`, `docs/governance` + `docs/registry` updates, cross-repo mirror fixtures.

## Non-Goals

- Hosted registry server control planes, worker pools, or cloud schedulers (styio-platform scope).
- SAT/backtracking multi-version resolution (unless REQ-RES-001's recorded decision changes scope first).
- Windows native implementation in this plan (REQ-PLAT-001 records the decision; the port is future work
  unless decided otherwise).
- Styio source parsing or compiler behavior (process-boundary integration only).
- Registry search/discovery and private-registry auth productization (tracked by convergence backlog and
  alpha checklist deferrals).

## Final Acceptance Target

On one head commit: delivery floor green (`delivery-gate.sh`, `native-check.sh`, ctest, Python unit,
interop, perf gate); the full adversarial suite green (forged TUF metadata, unauthenticated/traversal
publish, zip-slip/symlink archives, PATH hijack, torn-write and two-process races, resolver
cycle/diamond vectors, cache corruption); receipts/payloads schema-validated with the Vityo mirror gate;
all twelve labels individually evidenced in the final-validation node.
