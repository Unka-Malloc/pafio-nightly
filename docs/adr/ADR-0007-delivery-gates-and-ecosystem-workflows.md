# ADR-0007: Delivery Gates and Ecosystem Workflows

**Purpose:** Record the functional decision that external `styio` interoperability and cross-repository package-manager acceptance are validated through executable handoff and sample workflow gates.

**Last updated:** 2026-06-28

## Status

Accepted.

## Supersedes

- `ADR-0022-styio-handoff-spec-and-black-box-gate.md`
- `ADR-0033-ecosystem-sample-workflow-gate.md`

## Context

`spio` and `styio` are separate components. The package manager can generate manifests, locks, source archives, compile plans, and toolchain state, but it cannot claim end-to-end compiler interoperability by prose alone.

The project also needs one practical acceptance baseline that exercises package-manager workflows across repository boundaries without depending on internal implementation details.

## Decision

`spio` defines the external `styio` handoff requirements through a concrete spec and validates them with a black-box executable gate.

The ecosystem sample workflow matrix is the cross-repository acceptance baseline. It exercises the core package-manager loop through sample projects, lock and resolver behavior, fetch or sync preparation, compile-plan generation, packaging, and the published `styio` handoff boundary.

Claims about compatibility require passing executable gates or an explicitly published compatibility matrix. Documentation may describe future work, but it must not claim implemented cross-repository behavior without gate evidence.

## Alternatives

Using prose-only handoff notes would be cheaper but would not detect CLI, JSON, path, exit-code, or compatibility drift.

Letting each repository define its own acceptance baseline would make ecosystem readiness ambiguous.

Running only unit tests would miss the package-manager behavior that appears only when manifests, locks, caches, toolchains, and external compiler boundaries are combined.

## Consequences

Cross-repository changes should update the handoff spec and sample gate together.

The package-manager maturity plan can use the sample workflow gate as a release-readiness signal.

Compatibility status remains evidence-based instead of aspirational.
