# pafio Integration Tests

**Purpose:** Describe the black-box integration tests that validate `pafio` against external dependencies such as a published `styio` executable or a shared registry origin through public contracts only.

**Last updated:** 2026-07-30

## Requirements

- Use `PAFIO_STYIO_BIN` to locate the compiler.
- Create an isolated temporary project workspace per test.
- Create an isolated temporary `PAFIO_HOME` per test.
- Use only fixture files under `fixtures/`.
- Never read compiler source files directly.

## Owner Contract Gates

- `pafio_workflow_gate`
- `pafio_extractability_gate`
- Styio compile-plan interoperability
- Platform publish/consume and worker interoperability
- Vityo local and hosted adapter interoperability

Primary preflight entry:

```text
./scripts/preflight-readiness-check.py --styio-bin /absolute/path/to/styio
```

The active Better Plan owns the fixed-revision gate definitions.
