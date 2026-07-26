# Spio Package Manager Roadmap — Validation Matrix

**Plan:** `package-manager-roadmap`
**Last updated:** 2026-07-11

Harness layers: **N** = native gtest (`ctest`), **I** = interop shell (`tests/interop/*.sh`),
**U** = Python unit (`tests/unit/test_*.py`), **P** = perf (`scripts/perf-gate.py`),
**X** = cross-repo (`local-ci-gate.yml`). Security checks are negative-path: they must prove rejection.

## Standing gates (every implementation node)

| Gate | Command |
|---|---|
| Configure/build | `cmake -S . -B build -G Ninja && cmake --build build` |
| Native suite | `ctest --test-dir build` (SecurityTests, LockTests, ProjectGraphTests, ...) |
| Native check wrapper | `./scripts/native-check.sh` |
| Delivery floor | `./scripts/delivery-gate.sh --mode checkpoint` |
| Python units | `python3 scripts/submit-gate.py --profile ci --json` |
| Perf gate | `python3 scripts/perf-gate.py` (vs `tests/perf/baseline.json`, 15% threshold) |
| Plan-state health | `python <better-plan>/scripts/manifest_tool.py validate docs/plan` |

## Per-label mapping

### REQ-SEC-001 — Native TUF chain (N, U)
- **N:** tamper each role file (`trust/root.json`, `timestamp`, `snapshot`, `targets`) in a fixture →
  `MaterializeRegistryPackage` fails with a stable error, no cache write.
- **N/U:** parity fixtures — native verifier and `validator.py` return identical accept/reject verdicts.
- **N:** offline materialization from previously verified cache succeeds.

### REQ-SEC-002 — Control-plane auth + confinement (I, U)
- **I:** unauthenticated / wrong-token publish → non-2xx, no side effect
  (`tests/interop/registry-v2-control-plane-http.sh`).
- **I:** absolute and `..` traversal `archive_path`/`manifest_path` rejected after canonicalization;
  only staging-dir inputs publish.
- **U:** failure responses carry correct HTTP status codes with structured JSON (SPIO-AUD-014 closed).

### REQ-SEC-003 — Universal prescan (N, I)
- **N/I:** zip-slip and symlink-escape fixtures fail on the **git** acquisition path with stable errors.
- **N:** a fixture whose `tar -tf` listing exceeds the old 1 MiB cap is fully scanned or fails closed.
- **N:** code evidence that registry and git paths call the same shared prescan routine.

### REQ-SEC-004 — Signed descriptors (N)
- **N:** unsigned, expired, and unknown-signer descriptor imports rejected with stable errors
  (SecurityTests negative cases); `--dev` local-trust path emits a loud warning.

### REQ-SEC-005 — Absolute-path subprocess (N)
- **N:** PATH-hijack simulation (planted `curl`/`git`/`tar` on `PATH`) fails to hijack; spawns use
  absolute paths with `search_path=false` (code evidence).

### REQ-CON-001 — Store locking (I)
- **I:** two parallel `spio sync` over one `SPIO_HOME` → one winner or clean lock error; stores
  byte-consistent afterward; no deadlock (lock order project→cache→trust).

### REQ-ERR-001 — Atomic writes (N)
- **N:** crash-mid-write simulation (LockTests power-fail case) leaves no torn `spio.lock`; every mutable
  state write routes through `AtomicFile` (code evidence).

### REQ-RES-001 — Resolver contract (N)
- **N:** cycle vector → diagnostic naming the full cycle path; diamond-conflict vector → both requirement
  chains printed; diamond-agreement vector resolves; workspace-member-overlap vector handled.
- **N:** golden lockfile tests prove currently-valid projects resolve unchanged.
- Doc check: version policy recorded in `docs/governance/Spio-Manifest-and-Lock-Conventions.md`.

### REQ-CACHE-001 — Verified CAS + vendoring (N, I)
- **N:** digest-mismatch entry detected on read; self-heals online; fails precisely offline.
- **I:** vendor export/import round-trips registry deps → fully offline `spio sync`.
- **N:** git snapshot materialization records a content digest; FNV keys are non-identity (code evidence).

### REQ-WF-001 — Receipts (N, U)
- **N/U:** `spio build/run/test` JSON receipts validate against `contracts/compile-plan` (and new
  receipt) schemas.

### REQ-IDE-001 — Vityo payloads (U, X)
- **U:** `project_graph v1` / `toolchain_state v1` payloads validate against `contracts/` schemas.
- **X:** cross-repo mirror gate green against the `vityo-nightly` fixture set
  (`scripts/styio-interface-gate.py` / ecosystem gate where siblings exist).

### REQ-PLAT-001 — Platform matrix (doc)
- Doc check: Windows scope decision recorded in `README.md` / `docs/BUILD-AND-DEV-ENV.md` and consistent
  with the REQ-CON-001 locking design; interop layer's POSIX binding acknowledged.

## Final end-to-end acceptance target

On one head commit, in `delivery-gate.sh` order:

1. Build + standing gates (native suite, native-check, delivery floor, Python units, perf gate) green.
2. Full **adversarial suite** green on the same commit: forged TUF metadata (each role), unauthenticated
   + traversal publish, zip-slip/symlink git archives, oversized tar listing, PATH hijack, torn-write
   power-fail, two-process concurrent sync, resolver cycle/diamond vectors, cache corruption offline.
3. Receipts/payloads schema-validated; Vityo mirror gate green where the sibling checkout exists.
4. Per-label pass/fail recorded with archived outputs; failures route to the owning implementation node.
5. `manifest_tool.py sync-plan docs/plan` then `validate docs/plan` — clean.

Notes: interop/control-plane tests run localhost-only, no external network; perf-gate noise follows its
documented re-run policy; the POSIX-bound interop layer is a recorded REQ-PLAT-001 limitation, not a
final-validation failure.
