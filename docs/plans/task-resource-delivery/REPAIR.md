# Pafio task/resource workflow repair

**Purpose:** Record reproduced output ownership, permanent real workflow coverage, local evidence and remaining joins.

**Last updated:** 2026-10-11

This implements the existing TASK-001/NODE-REPAIR in hosted E06. The accepted
BASELINE.md and runtime `c63ce9f73fe57161550e827846311798` were reused. The source
Task/Tree remain incomplete. Only `pafio-task-resource-focused` is selected for
this repair node; local commands below are not platform check results.

The independent [HANDOFF.md](HANDOFF.md) records the subsequent source review,
mandatory observation coverage, current upstream dependencies and evidence.
The original commands below retain their executed scope.

## Candidate and implementation

The existing branch `agent/downstream-pafio-paths`, base revision
`a20dce6e9d889e92`, local documentation edits and `build-codex` are preserved.
The candidate includes uncommitted source, tests and formal documentation.
Use `build-codex/bin/pafio`, with explicit compiler selection of
`../styio-nightly/build/default/bin/styio`. The associated daemon is
`../styio-nightly/build/default/bin/styio_lspd`; it was not run here. No upstream
source was changed or rebuilt. Styio HANDOFF.md, REPAIR.md and COST.md define
that accepted dirty candidate, not its base revision alone.

`WriteBuildCompilePlan` now atomically allocates
`.pafio/build/<cache_key>/<invocation>/` for every plan, including dry-run.
Cache material and identity are unchanged. Each plan/diagnostic/receipt/artifact
path has one invocation owner; prior results are retained. The opaque receipt
boundary is unchanged: production Pafio checks materialization, not schema
contents. No parser, old-layout fallback, cache identity version or deletion
migration was added. Existing old directories are independent retained artifacts,
not an executable compatibility path.

The allocation pattern follows exclusive filesystem creation, as in
[LLVM's createUniqueDirectory implementation](https://github.com/llvm/llvm-project/blob/main/llvm/lib/Support/Path.cpp),
and reuses this repository's clock/sequence naming approach from AtomicFile.cpp.
A name collision retries directory allocation; an actual filesystem error fails
with PlanError. No arbitrary retry deadline or hash scan is introduced. Each
invocation adds its own directory and outputs; there is no performance or storage
reduction claim. Retention is intentional because delayed consumers and parent
snapshots may still need earlier artifacts. No new cleanup policy was invented.
The fixed 60-minute compile-process timeout and its unused constant were removed;
healthy compilation runs to completion. Other process policies were not redesigned.

## Corrected finite ledger

| ID | Classification and evidence | Disposition |
| --- | --- | --- |
| P1/P2/P6 | Integration coverage gaps, not reproduced runtime defects. Old gate executed Styio directly after Pafio dry-run; scripted interop tests are plumbing evidence only. | Gate now invokes real Pafio; task/resource fixtures, focused/integration entrypoints and opt-in real CTest registration are permanent. |
| P3 | Receipt opacity is intentional. Missing parsing was not a defect. Current-result tests were absent. | Keep opacity; test-side receipt assertions verify current identity, outputs, diagnostics and artifacts. Actual ownership defect is P4. |
| P4 | Reproduced: two real successful builds returned identical plan paths but produced distinct compiler session IDs. Reading the first success envelope after the second returned the second receipt. Real concurrent builds also shared outputs. | Allocate per-invocation output roots. Permanent tests retain old result bytes, assert equal cache identity after repair, distinct sessions and separate concurrent output paths. |
| P5 | Coverage gap for upstream language behaviors. No new language defect reproduced by the required unobserved workflow. | Copied authoritative t13/t22/t23/state-e06/task-e06 inputs and expectations; real run/test assertions cover history, fallback, unhandled failure and f64. |
| U1 | Additional upstream consumer defect: real snapshot request fails because Styio expects `packages[].root`/`manifest`, while Pafio's canonical v1 schema and producer emit `root_dir`/`manifest_path`. | Reported to coordinator; no producer alias or upstream patch. Snapshot join remains unavailable pending upstream repair/review/rebuild. Runtime observation shares that identity path by source inspection but was not separately executed. |

P4 evidence: `receipt-replacement.json`, `concurrent-0.json` and
`concurrent-1.json` under the evidence directory below. Both sequential build
commands exited 0; the saved receipt session values differed while the returned
paths were equal. This is an output ownership reproduction, not proof of a
compiler schema-parsing defect or a reproduced concurrent crash.

U1 evidence: `first.stderr` and `second.stderr`; both real snapshot checks exited
23 with `STYIO_SERVICE_COMPILE_PLAN_INVALID` / `observable_static_snapshot_anonymous`.
An initial scratch manifest omitted its mandatory build table (exit 10); that
probe-input error was corrected before these calls and is not a product defect.
The owner mismatch is at Pafio schema package fields and Styio
`src/StyioServices/StyioConfig/CompilePlanContract.cpp:599-601`.

## Permanent verification surface

`scripts/styio-interface-gate.py --require-compile-plan` now performs Pafio build.
`--task-resource` adds the complete real workflow on one reusable project, with
an isolated PAFIO_HOME and explicit compiler selection that overrides an invalid
environment compiler. Gate-side receipt interpretation is test code only.
`tests/integration/fixtures/task-resource/` contains byte copies of the accepted
Styio inputs and expectations; the fixture README records their provenance.
No generated task syntax or fake compiler proves this workflow.

The shared project is `build-codex/task-resource-focused/project/`, restored to
t13 after the gate. The manifest declares one binary and one test over the same
source. It is suitable for the coordinator's next same-project Vityo scenario.
The CMake cache option `PAFIO_TEST_STYIO_BIN` registers
`pafio_task_resource_real_workflow`; without a selected compiler it is absent,
not passed. `checks.sh focused` builds changed targets, runs the scoped native
selection and the real gate. `integration` builds Pafio and runs the independent
real gate for HANDOFF, without duplicating the native selection.

The gate requires exact runtime stdout plus the Pafio completion line, compiler
identity/capability agreement, current diagnostic messages, no failure receipt,
and no compile-error artifacts. Runtime failure can retain its own compiled IR;
that is current failure evidence, not a successful receipt. Earlier outputs must
remain byte-identical throughout the sequence and dry-run. Concurrent builds
must have equal cache keys and different roots/sessions.

## Actual local commands

Evidence paths in this section are relative to
`build-codex/task-resource-repair-20261011/`. Commands redirected directly to
files preserve the command exit status; no successful tail/tee exit substituted
for build or test status.

| Command | Actual result | Evidence |
| --- | --- | --- |
| `cmake --build build-codex --target pafio pafio_native_tests` | Exit 0 after source/test changes | `build.log` |
| `build-codex/bin/pafio_native_tests --gtest_filter='BuildPlanTests.*:BuildCliTests.*:RunCliTests.*:TestCliTests.*:CheckCliTests.*:SyncCliTests.*'` | Exit 0, 39 tests passed; includes dependency sync/offline/vendor, locked/frozen and plan/CLI behavior | `native-focused.log` |
| Real task/resource gate during development | Three assertion failures: receipt has extra `runtime_events_path`; capability order is normalized; runtime failure retains current IR. Test assumptions corrected to owner contracts; no product workaround. | `workflow.json`, `workflow-corrected.json`, `workflow-capabilities.json` |
| `cmake --build build-codex --target pafio pafio_native_tests` (final) | Exit 0 after removing the now-unused timeout constant | `build-final.log` |
| `cmake -S . -B build-codex -DPAFIO_BUILD_TESTS=ON -DPAFIO_TEST_STYIO_BIN=<selected-styio>` | Exit 0; real CTest registered against explicit candidate | `configure-real-test.log` |
| `bash docs/plans/task-resource-delivery/checks.sh focused` | Local exit 0: build, 39 native tests and 23 gate subprocess steps with expected exits | `focused-local.log`, extracted `focused-workflow.json` |
| `ctest --test-dir build-codex --output-on-failure --no-tests=error -R '^pafio_task_resource_real_workflow$'` | Exit 0, 1 real workflow CTest passed after final helper isolation and candidate rebuild | `real-ctest.log`; detailed CTest output under `build-codex/Testing/Temporary/LastTest.log` |
| `bash tests/interop/styio-interface-gate-compile-plan.sh <pafio>` | Exit 0; scripted executor plumbing only | `compile-plan-plumbing.log` |
| `bash tests/interop/styio-interface-gate-handshake.sh <pafio>` | Initial obsolete `pafio_check`/`styio` envelope expectation failed; corrected to actual doctor checks/detail fields; rerun exit 0 | `handshake.log`, `handshake-corrected.log` |

Every subprocess in the successful real gate records its exact argv, stdout,
stderr and exit in `focused-workflow.json`. Compact per-command results:

| Step(s), each executed once in that gate | Exit(s) | Assertion |
| --- | --- | --- |
| Styio machine-info; Pafio doctor | 0; 0 | Actual selected compiler contract |
| success check / build / test / run | 0 / 0 / 0 / 0 | Current outputs; run/test parent 9, captured 7 |
| compile_error check / build / test / run | 23 / 23 / 23 / 23 | Non-task order diagnostic; no receipt/artifacts |
| repaired check / build / test / run | 0 / 0 / 0 / 0 | Same original cache identity; new roots and no stale errors |
| fallback run / test | 0 / 0 | `saved`, `99`, `saved` |
| f64 run / test | 0 / 0 | `1.110223` |
| no-fallback run / test | 23 / 23 | Division failure settles, current diagnostic, no success receipt |
| build dry-run | 0 | planned/not_run; preserves all earlier output bytes |
| concurrent build 0 / build 1 | 0 / 0 | Real overlapping processes, independent roots and sessions |

## Exact HANDOFF and Vityo join requirements

HANDOFF independently reviews production source, permanent tests and evidence;
it must run only its admitted `pafio-task-resource-integration` platform check.
The coordinator selects the same dirty Pafio and Styio CLI/daemon artifacts
above, including any subsequently reviewed U1 repair, and the reusable project.
No full regression is authorized before all three source reviews and that common
same-candidate Vityo E2E.

Vityo must use actual production services and vityod, with explicit external
binary configuration. Exercise save/diagnostics and check/build/test/run for
success → compile failure → repaired success, history/f64 and both task failure
modes; verify the selected styio_lspd path and its required LSP interaction.
Current-success selection must be invalidated on failure, with current diagnostics
shown and no adoption of an earlier receipt/artifact. Dry-run must preserve the
previous result. Concurrent and delayed consumers must each follow their own
returned `plan` paths, never a derived `.pafio/build/<cache_key>` path.

Read-only consumer inspection found `execution_adapter_io.dart` and
`observable_snapshot_publisher_io.dart` already use returned `build_root`.
That is a source fact, not Vityo validation. The Vityo owner must verify those
paths, receipt loaders, failure-state invalidation and tests against the extra
opaque invocation component, preserving earlier parent snapshots. Repair U1 in
its Styio owner before claiming observed/snapshot workflows. Pafio's `--json`
failure remains the existing stderr error envelope; it does not expose a success
plan or inline receipt. Consumers must handle that documented boundary rather
than reusing a prior success envelope. No sibling edits were made here.

## Verifier addendum (2026-10-11, independent re-verification)

Independent source review and re-execution on the rebuilt B5 Styio candidate
(`../styio-nightly/build/default/bin/styio` relinked after the upstream
root_dir/manifest_path repair) confirms the implementation above.

- `bash docs/plans/task-resource-delivery/checks.sh focused` exited 0:
  incremental build, 39 native tests and all 23 real gate steps
  (`focused.log` under `build-codex/task-resource-verifier-20261011/`).
- The five `.styio` fixtures are byte-identical to the accepted upstream
  inputs; the five `.expected` files are byte-identical to upstream
  `.out`/`.err` expectations (rename documented in the fixture README).
- P4 ownership repair verified by the gate: equal cache keys, distinct
  invocation roots and sessions for sequential and real concurrent builds,
  earlier result bytes preserved, dry-run keeps prior outputs, failed
  invocations own plan+diagnostics without a receipt.
- U1 verified repaired: a real `pafio check --emit-observable-static-snapshot`
  on the B5 candidate no longer fails at the package field stage.
- New upstream defect U2 reported to the coordinator: the
  same real command now exits 23 with
  `STYIO_SERVICE_COMPILE_PLAN_INVALID`/`observable_static_snapshot_anonymous`,
  `compile-plan package name must be a namespaced identity and must not be
  path-shaped`. `--emit-runtime-observation` fails identically because both
  emission requests share `admit_qualified_compilation_unit`
  (upstream `CompilePlanContract.cpp:903-911` → `package_name_is_valid:207`).
  Pafio's manifest contract requires `namespace/name`
  (`src/PafioManifest/Manifest.cpp` `^[a-z0-9][a-z0-9_-]*/[a-z0-9][a-z0-9_-]*$`),
  so every valid Pafio package fails this admission; `packages[].name` is the
  canonical manifest name and cannot be path-normalized producer-side without
  a contract decision. Evidence: `snap1.err`, `rtobs.err` and `snap-proj/`
  under the verifier evidence directory. Observation validation stays
  upstream-blocked; the unobserved workflow contract here is unaffected.

| File/line | Rule | Redacted content/category | Basis | Impact and disposition |
| --- | --- | --- | --- | --- |
| No finding | Local-home/private-key/token/private-address patterns | No matching content | New fixture and planning files rescanned; ignored evidence excluded | No remediation |

## Limits and privacy

No platform-check pass, HANDOFF review, Vityo E2E, LSP run, complete regression,
other-platform/Release/sanitizer verification, publication, installation or live
Agent/UI acceptance is claimed. Source Task/Tree delivery remains null. Baseline
results were reused, not rerun. Original inputs and existing user edits remain.
Raw tool paths, command output and compiler session data stay in ignored build
directories; shared-board notes contain only relative references and safe findings.

A local pattern scan covered tracked modifications and unignored new files for
local home paths, private-key blocks, common token literals and private IPv4
addresses. It returned no findings (`privacy-findings.json`); changed text was
also reviewed for raw runtime payload additions. This is scoped evidence, not a
universal secret-detection claim. A one-off migration check found no retired
active output-layout/timeout assertions, and byte-compared all five copied inputs
and expectations (`migration-and-fixtures.txt`). Neither check was installed as
a permanent regression gate. `git diff --check` and Python AST parsing passed.

| File/line | Rule | Redacted content/category | Basis | Impact and disposition |
| --- | --- | --- | --- | --- |
| No finding | Local-home/private-key/token/private-address patterns | No matching content | Changed and new source/document/fixture files scanned; ignored evidence excluded | No remediation; raw machine data remains ignored |
