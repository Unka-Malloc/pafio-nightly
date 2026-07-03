# Styio Spio Test Framework Defects 2026-04-29

**Purpose:** Record test framework and test-case defects found during the 2026-04-29 review of `styio-spio`.

**Last updated:** 2026-04-29

**Status:** Open
**Scope:** `styio-spio` native, CLI, integration, package-manager, submit-gate, and CI test coverage.
**Evidence reviewed:** `tests/CMakeLists.txt`, `tests/README.md`, `tests/integration/README.md`, `tests/integration/scripts/run-blackbox-placeholder.sh`, `.github/workflows/local-ci-gate.yml`, `scripts/delivery-gate.sh`, `scripts/checkpoint-health.sh`, and `scripts/submit-gate.py`.

This record captures test framework and test-case gaps. The repository has meaningful native and CLI coverage, but several release-critical workflows are still represented by placeholder or optional gates.

## Findings

| ID | Severity | Area | Defect | Evidence | Required closure |
| --- | --- | --- | --- | --- | --- |
| SPIO-TEST-001 | High | Integration coverage | The black-box integration runner is still a placeholder, so the documented external `styio` subprocess contract is not actually enforced by a real integration suite. | `tests/README.md` describes integration coverage; `tests/integration/scripts/run-blackbox-placeholder.sh` states that it must be replaced by real integration runners. | Replace the placeholder with runnable black-box scenarios for install, resolve, lockfile, offline/cache, failure, and rollback paths. Register them in CTest and CI. |
| SPIO-TEST-002 | High | CI parity | The local CI workflow invokes `delivery-gate.sh --skip-audit --skip-health`, then relies on `submit-gate.py`. This makes the push/PR path differ from the checkpoint closure path and can leave external audit or health regressions outside the required CI signal. | `.github/workflows/local-ci-gate.yml` uses `--skip-audit --skip-health`; `scripts/delivery-gate.sh` treats audit and checkpoint health as first-class checkpoint gates. | Align CI with checkpoint mode or document and enforce an equivalent submit-gate profile that includes the skipped health and audit contracts. |
| SPIO-TEST-004 | Medium | Private release gates | Optional `tests-private` coverage and auth-bearing registry paths are not discoverable as a required release signal in the public gate. A release can look complete while private registry/publish checks were never run. | `tests/CMakeLists.txt` includes `tests-private/CMakeLists.txt` only when it exists; `scripts/submit-gate.py` keeps some release/profile feature flags disabled unless configured. | Add a visible required-check marker for private release gates, or fail release profiles when required private/auth scenarios are unavailable. |
| SPIO-TEST-005 | Medium | Negative/package-manager cases | Public package-manager tests cover many CLI contracts, but the integration layer does not yet assert corrupted registry metadata, partial downloads, cache poisoning, network failure retry, or rollback behavior as end-to-end cases. | Current registered tests are concentrated in native/unit/CLI contract suites; the integration runner remains placeholder. | Add end-to-end package-manager failure-mode tests and make them part of the release or checkpoint profile. |

## Closure Expectations

Before marking this record closed:

- Replace the placeholder integration runner with real black-box package-manager scenarios.
- Remove or justify `--skip-audit --skip-health` from the CI gate path.
- Make private release-gate availability visible and blocking for release profiles.
- Re-run `./scripts/delivery-gate.sh --mode checkpoint` and the CI-equivalent submit gate after the fixes.
