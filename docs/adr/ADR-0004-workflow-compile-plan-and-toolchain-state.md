# ADR-0004: Workflow, Compile Plan, and Toolchain State

**Purpose:** Record the functional decision that build, run, test, reproducibility flags, managed `styio` installation, project-local workflow state, and local cloud policy are one workflow layer.

**Last updated:** 2026-06-28

## Status

Accepted.

## Supersedes

- `ADR-0012-phase4-build-dry-run-compile-plan.md`
- `ADR-0013-phase4-run-dry-run-and-test-gap.md`
- `ADR-0014-phase4-test-dry-run-with-explicit-test-targets.md`
- `ADR-0017-phase6-managed-local-styio-tool-install.md`
- `ADR-0018-phase6-managed-styio-version-switching.md`
- `ADR-0019-phase6-reproducible-workflow-flags-and-project-local-vendor.md`
- `ADR-0020-phase6-project-local-managed-styio-pinning.md`
- `ADR-0034-local-cloud-execution-policy-contract.md`
- `ADR-0036-prebuilt-first-installer-and-managed-shim.md`
- `ADR-0037-project-local-workflow-state.md`

## Context

Once manifest, lock, and resolver behavior are stable, package-manager value moves to repeatable workflows: build, run, test, source preparation, toolchain selection, and policy rendering. These behaviors all depend on the same selected package graph, selected target, selected compiler, and project-local state.

The project also needs to avoid implying a hosted scheduler, remote worker pool, or fully managed compiler service before those systems exist.

## Decision

`spio build --dry-run`, `spio run --dry-run`, and `spio test --dry-run` emit local compile-plan v1 payloads under project-local `.spio/build/<cache-key>/plan.json`.

`spio test` requires explicit `[[test]]` targets. The package manager does not synthesize fake test targets or over-claim compiler execution maturity.

The reproducibility surface includes `--locked`, `--offline`, `--frozen`, and project-local vendoring under `.spio/vendor/`.

Managed `styio` installation is package-manager state rooted under `SPIO_HOME/tools/styio/`. Installation, activation, and project-local pinning are separate operations exposed through `spio tool install`, `spio tool use`, and `spio tool pin`.

Alpha bootstrap is prebuilt-first for `spio`, then exposes a managed `styio` compiler through a shim. Project-local workflow state records mode, channel, source revision, build mode, and execution policy.

`spio cloud status` and `spio cloud plan` are local policy-contract renderers. They define vocabulary, selected lane, risk, security policy, worker-pool key, and cache policy, but they do not imply a live remote queue, scheduler, or worker manager.

## Alternatives

Running the compiler directly before dry-run plan generation was complete would collapse planning, compatibility, and execution into one hard-to-debug command path.

Treating compiler selection as only an environment-variable convention would make builds depend on untracked machine state.

Implementing `cloud` commands as placeholders for a future hosted service would misrepresent the current open-source package-manager boundary.

## Consequences

Compile-plan generation, toolchain selection, and local policy rendering must share project-local state semantics.

Non-dry-run execution can grow from this layer, but it must preserve receipts, selected compiler identity, compile-plan hashes, selected target identity, and artifact evidence.

Cloud-related commands must stay explicit about the difference between local contract rendering and remote execution.
