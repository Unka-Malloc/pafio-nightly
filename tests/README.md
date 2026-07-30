# pafio Tests

**Purpose:** Define the split between `pafio` unit tests and black-box integration tests, with strict environment and cache isolation.

**Last updated:** 2026-07-30

## Test Classes

- `unit/` covers repository scripts and fixture-level policies.
- native `C++20` coverage under the CMake test target is authoritative for product behavior.
- `integration/` covers real subprocess execution against an external `styio` binary.

## Isolation Rules

- Every test run must set a fresh temporary `PAFIO_HOME`.
- Integration tests must use `PAFIO_STYIO_BIN` and must not assume a source checkout of `styio`.
- Tests must not write into the repository root except under explicit temporary directories created for the run.

Use focused CTest filters during implementation. Cross-repository compiler,
Platform, and Vityo acceptance runs from the fixed-revision product matrix.
Fixtures and logs must not contain credentials or backend runtime data.
