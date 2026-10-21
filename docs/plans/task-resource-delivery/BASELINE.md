# Current-source baseline evidence

**Purpose:** Record the verified current-source build, supported inventory, and finite issue ledger for the next stage.

**Last updated:** 2026-10-11

**Observed:** 2026-10-11 · **Owner:** NODE-BASELINE / TASK-001
**Registered check:** `pafio-task-resource-baseline` only

This is engineering evidence for the existing programme, not candidate
acceptance or Task/Tree delivery. No commit, push, pull request, installation,
publication, sibling-repository edit, or full regression was performed.

## Candidate and actual build inputs

- Revision `a20dce6e9d889e92`, existing branch `agent/downstream-pafio-paths`,
  existing `build-codex` directory.
- Changed inputs: tracked `docs/INDEX.md` and `docs/plan/README.md` edits and
  untracked `docs/plans/` workspace. All `src/` and `tests/` content is clean
  at HEAD; no implementation change is pending in this baseline.
- Registered command `bash docs/plans/task-resource-delivery/checks.sh baseline`
  exited 0: configure plus incremental build produced zero compile/link
  actions because the existing binaries already cover HEAD (linked
  2026-10-05, same-session commit, clean worktree since). Incremental reuse is
  valid; the binary is current-source evidence.
- Actual configuration: macOS arm64, Debug, Unix Makefiles, CMake 4.3.1,
  `PAFIO_BUILD_TESTS=ON`. Other platforms, Release and sanitizer variants were
  not verified.
- `build-codex/bin/pafio` reports `pafio 0.1.0-dev`; `machine-info` advertises
  compile_plan/metadata/resolution/workflow v1 and manifest/lock v1.
- Compiler candidate: coordinator-selected `../styio-nightly/build/default/bin/styio`
  and `styio_lspd`, consumed in place. `pafio doctor` against it exited 0
  (`0.0.1`/`nightly`, compile_plan `[1]`, capabilities satisfy the nightly
  compat entry). A dry-run `pafio build` on a scratch project under the
  ignored build directory emitted compile-plan v1 with `status=planned` and
  `styio.status=not_run`; no compiler is invoked for dry-run.

## Supported-capability and platform inventory

- CLI surface: `machine-info`, `doctor`, `metadata`, `new`/`init`, `check`,
  `add`, `remove`, `sync`, `tree`, `vendor`, `build`, `run`, `test`, `pack`,
  `publish`, `registry trust import|status`.
- Workflow contract: `check`/`build`/`run`/`test` sync dependencies, emit
  compile-plan v1, and execute `styio --compile-plan`. Discovery order is
  `--styio-bin` → `PAFIO_STYIO_BIN` → `PATH`. The compat matrix requires a
  channel/version match, `machine_info_json` + `single_file_entry` +
  `jsonl_diagnostics`, compile-plan v1, `edition_max >= 2026`.
- Offline contract: `--locked` forbids lock mutation, `--offline` forbids
  network, `--frozen` is both; `vendor`/`pack` provide local state. Pafio
  never installs or mutates Styio.
- Registered coverage: 146 CTest cases across `pafio_native_tests` suites
  (BuildPlan, BuildCli, Check, Sync, Run, Test, Manifest, Lockfile,
  LockGeneration, Resolver, Tree, Add, Remove, Metadata, Pack, Publish,
  Security, TufVerifier, Vendor, ResolutionContract, Paths, Process,
  AtomicFile, FileLock, Doctor), `pafio_process_tests` (`PortableProcess.*`),
  and 19 `pafio_cli_*` shell probes; plus unit Python gate tests. Interop
  scripts under `tests/interop/` use scripted fake `styio` executors.
- Platforms: this baseline verifies macOS arm64 Debug only. CI declares a
  blocking Linux Debian 13 gate, a macOS smoke lane, and a dispatch-only
  Windows lane; none were exercised for this milestone.

## Current C1 / current-results contract state

- Success emits compiler identity, capabilities and probe results for the
  current invocation; failure emits `CompilerError`; dry-run reports
  `not_run`.
- Output roots are deterministic: `.pafio/build/<cache_key>` keyed on compiler
  identity, intent, edition, profile, target, source hash and observation
  requests. Dry-run writes `plan.json` and creates dirs without invalidating a
  prior `receipt.json`; real runs require receipt existence only after exit 0
  and do not parse receipt identity/freshness.
- Producer-side freshness comes from the repaired Styio B4 (prior receipt
  invalidated before diagnostics; receipts only on success). Pafio-side
  stale-result assertions do not exist yet — see P3.

## Finite issue ledger

Classification corrected for recovery E06: P1/P2/P5/P6 are integration coverage
gaps, not reproduced product defects. Receipt opacity in P3 is intentional.
P4 was a source-level risk at baseline; subsequent reproduction and repair are
recorded in REPAIR.md. This correction does not replay the accepted baseline.


| ID | Severity | Status | Finding | Owning stage |
| --- | --- | --- | --- | --- |
| P1 | coverage | open→repair | `styio-interface-gate.py` runs `doctor` + `build --dry-run`, then executes `styio --compile-plan` directly; it never drives real `pafio check/build/test/run`. `checks.sh` `focused` and `integration` are byte-identical. | NODE-REPAIR / NODE-HANDOFF |
| P2 | coverage | open→repair | Non-dry-run CLI tests and interop gates use fake `styio`; no permanent test drives real `pafio check/build/test/run` against a real compiler. | NODE-REPAIR |
| P3 | coverage | open→repair | Pafio intentionally keeps receipt contents opaque and checks existence after exit 0; success → compile-error → repaired-success and old-artifact isolation lack permanent Pafio tests. | NODE-REPAIR |
| P4 | investigation | reproducer required at baseline | Deterministic `build_root` means identical concurrent invocations share output paths and `plan.json` writes are non-atomic; upstream requires callers not to share output paths, and malformed plans without recoverable output roots require caller invalidation. | NODE-REPAIR evaluates; document for NODE-HANDOFF |
| P5 | coverage | noted | Task creation-time history, task failure with/without fallback and f64 are language behaviors repaired upstream; no Pafio project fixture exercises them through check/build/test/run. | NODE-REPAIR / NODE-HANDOFF |
| P6 | coverage | noted | `tests/integration/` declares real-subprocess tests but contains only READMEs; no registered real-executor tests. | NODE-REPAIR |
| P7 | info | closed | Stale-looking binary resolved: `build-codex/bin/pafio` is a valid current-source build; incremental reuse confirmed. | NODE-BASELINE |

## Evidence and limits

Raw logs and the scratch project remain under the ignored
`build-codex/task-resource-baseline-20261011/` directory:
`baseline-checks.log`, `doctor-selected-styio.log`, `dry-run-probe.log`,
`check-result.json`, `node-result.json`, `probe-proj/`.

Not established here: no focused/integration gate run, no full regression, no
source edits, no Vityo join. NODE-REPAIR owns gate/test extension and defect
closure; the coordinator owns cross-repository handoff timing.
