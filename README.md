# spio

**Purpose:** `spio` is the standalone package manager and project workflow tool for Styio. It is designed to remain movable as a self-contained subtree and later as an independent repository.

**Last updated:** 2026-04-20

## Scope

- `spio` manages package manifests, lockfiles, dependency resolution, cache layout, build orchestration, and project-level commands.
- `spio` does not parse Styio source semantics on its own.
- `spio` supports two project toolchain modes:
  - `binary`: published compiler path through a versioned machine contract and a process boundary
  - `build`: source-build path through an official `styio` source checkout and local compiler build cache

## Independence Rules

- `spio` must not include or link against `styio` implementation headers or libraries.
- `spio` must not depend on files under `../src`, `../tests`, or any other compiler-internal path.
- `spio` may depend on a published external `styio` executable only through the documented binary-mode discovery path such as `--styio-bin` or `SPIO_STYIO_BIN`.
- `spio` source-build mode may fetch the official `styio` source tree from `https://github.com/eBioRing/Styio.git`, using the `stable` and `nightly` branches as the channel roots, through the documented source-build contract and cache layout.
- `spio/contracts/` is the source of truth for package-manager-side machine contracts.

## Tree

```text
spio/
  src/
  tests/
    unit/
    integration/
  docs/
  contracts/
  scripts/
```

## Transitional Note

The current repository root still hosts the existing `styio` compiler project directly. This `spio/` subtree is being prepared so it can later be moved wholesale into `/Users/unka/DevSpace/Unka-Malloc/styio-spio` without dragging compiler source code along with it.

## Implementation Stack Note

The active implementation target is a native `C++20` + `CMake` codebase aligned with the operational toolchain used by `styio`.

The existing Python bootstrap remains in-tree only as a temporary migration reference while native phase-2 parity is being built for:

- CLI shape
- manifest and lockfile validation rules
- machine-facing contract boundaries

Python is not the intended long-term implementation path for `spio`.

## Developer Context Pack

Before moving this subtree into `/Users/unka/DevSpace/Unka-Malloc/styio-spio`, `spio` developers should read:

- `docs/styio/Styio-for-Spio-Developers.md`
- `docs/styio/Styio-Public-Interface-Roadmap.md`
- `docs/governance/Spio-Version-Decoupling-Constraints.md`

Those documents are the migration knowledge pack for working against `styio` without creating hidden source-level dependencies.

## Developer Entry Points

Start repo bootstrap and common build/test commands from [docs/BUILD-AND-DEV-ENV.md](docs/BUILD-AND-DEV-ENV.md).

Project-local workflow mode selection now uses:

- `./scripts/spio use binary`
- `./scripts/spio use build`
- `./scripts/spio set channel as stable`
- `./scripts/spio set channel as nightly`
- `./scripts/spio set build as minimal`
- `./scripts/spio set risk as trusted-internal|partner-controlled|untrusted-user`
- `./scripts/spio set lane as isolated|warm-shared`
- `./scripts/spio set security as sandbox-default|partner-restricted|trusted-warm`
- `./scripts/spio cloud status --json`
- `./scripts/spio build minimal`

统一 docs/process 与交付入口分别为：

- `./scripts/docs-gate.sh`
- `./scripts/checkpoint-health.sh`
- `./scripts/delivery-gate.sh --mode checkpoint`

## Planning Entry Points

For the full implementation and migration plan, start with:

- `docs/planning/Spio-Master-Plan.md`
- `docs/planning/Spio-Workstreams-and-TODOs.md`
- `docs/operations/Spio-Verification-Matrix.md`
- `docs/operations/Spio-Repo-Split-Runbook.md`

Recommended preflight before moving this subtree:

```text
./scripts/bootstrap-dev-env.sh
./scripts/preflight-readiness-check.py --styio-bin /absolute/path/to/styio
```
