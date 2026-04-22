# spio

**Purpose:** `spio` is the standalone package manager and project workflow tool for Styio. It is designed to remain movable as a self-contained subtree and later as an independent repository.

**Last updated:** 2026-04-21

## Scope

- `spio` manages package manifests, lockfiles, dependency resolution, cache layout, build orchestration, and project-level commands.
- `spio` does not parse Styio source semantics on its own.
- `spio` supports two project toolchain modes:
  - `binary`: published compiler path through a versioned machine contract and a process boundary
  - `build`: source-build path through an official `styio` source checkout and local compiler build cache

## Product Surface Split

- `frontend/console/` is reserved for the repo-hosted human control console page.
- `src/` remains the backend/domain core for package, registry, toolchain, and cloud-platform behavior.
- `docs/registry/` and `docs/governance/` remain the SSOT for service-side contracts.
- the native CLI stays the machine/admin surface; it is not the user-facing control console.

The normative split is defined in
[`docs/governance/Spio-Control-Console-And-Service-Split.md`](docs/governance/Spio-Control-Console-And-Service-Split.md).

## Native Target Split

- `src/` no longer builds as one monolithic `spio_core`.
- backend/domain code now composes from internal static libraries such as `spio_foundation`, `spio_manifest`, `spio_resolution`, `spio_toolchain_service`, `spio_package_service`, and `spio_project_service`.
- the CLI surface now sits on top as `spio_cli_support`, `spio_cli_commands`, and `spio_cli_shell`, with the `spio` executable linking the shell target only.
- this keeps future backend/service binaries free to reuse the backend-side libraries without inheriting CLI routing code or any repo-hosted console UI concerns.

The source-level ownership summary lives in
[`src/README.md`](src/README.md) and the planning note for the split lives in
[`docs/planning/Spio-Native-Target-Split.md`](docs/planning/Spio-Native-Target-Split.md).

## Independence Rules

- `spio` must not include or link against `styio` implementation headers or libraries.
- `spio` must not depend on files under `../src`, `../tests`, or any other compiler-internal path.
- `spio` may depend on a published external `styio` executable only through the documented binary-mode discovery path such as `--styio-bin` or `SPIO_STYIO_BIN`.
- `spio` source-build mode may fetch the official `styio` source tree from `https://github.com/eBioRing/Styio.git`, using the `stable` and `nightly` branches as the channel roots, through the documented source-build contract and cache layout.
- `spio/contracts/` is the source of truth for package-manager-side machine contracts.

## Tree

```text
spio/
  frontend/
    console/
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

The native core is now the active implementation path for:

- CLI shape
- manifest and lockfile validation rules
- machine-facing contract boundaries
- registry `v2` static distribution and control-plane contract gates

Python remains in-tree only where it owns repository automation, contract gates, and registry/control-plane helper tooling.

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
- `./scripts/spio project-graph --json`
- `./scripts/spio cloud status --json`
- `./scripts/spio cloud plan --json build minimal`
- `./scripts/cloud-compile-stress.py --require-hot-replacement --summary-json /tmp/spio-cloud-stress-summary.json --events-jsonl /tmp/spio-cloud-stress-events.jsonl`
- `./scripts/spio tool status --json`
- `./scripts/spio build minimal`

`./scripts/spio` is the repository-local convenience wrapper. It ensures the native binary exists under `./build-codex/bin/spio` and then forwards the remaining arguments. Use the wrapper in day-to-day developer docs; use the explicit binary path when a gate or external harness needs a concrete executable.

Current source-build and cloud boundary:

- `build` mode is implemented as a local source-build workflow rooted in the official `https://github.com/eBioRing/Styio.git` source tree.
- `cloud status` and `cloud plan` are implemented as local machine-readable contract surfaces.
- the tracked open-source tree does **not** yet ship the future remote async control plane, queue, or worker pools.

统一 docs/process 与交付入口分别为：

- `./scripts/docs-gate.sh`
- `./scripts/checkpoint-health.sh`
- `./scripts/delivery-gate.sh --mode checkpoint`

## Planning Entry Points

For the full implementation and migration plan, start with:

- `docs/planning/Spio-Master-Plan.md`
- `docs/planning/Spio-Stage-Review-and-Future-Features.md`
- `docs/planning/Spio-Workstreams-and-TODOs.md`
- `docs/operations/Spio-Verification-Matrix.md`
- `docs/operations/Spio-Repo-Split-Runbook.md`

Recommended preflight before moving this subtree:

```text
./scripts/bootstrap-dev-env.sh
./scripts/preflight-readiness-check.py --styio-bin /absolute/path/to/styio
```
