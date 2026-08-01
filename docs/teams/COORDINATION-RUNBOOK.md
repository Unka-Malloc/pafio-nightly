# Coordination Runbook

**Purpose:** Route Pafio changes across core, registry, compiler-contract, and delivery owners.

**Last updated:** 2026-07-30

## Mission

Keep every closure inside one Pafio product boundary and coordinate external
owners through published machine contracts.

## Module Map

```text
Core / Workflow -> Styio / Contracts
Core / Workflow -> Registry / Publish
Docs / Delivery -> all Pafio owners
Styio / Contracts -> Styio
Registry / Publish -> Styio Platform
```

## Ownership Table

| Team | Runbook | Primary surface |
| --- | --- | --- |
| Core / Workflow | `CORE-WORKFLOW-RUNBOOK.md` | project and dependency transactions |
| Registry / Publish | `REGISTRY-PUBLISH-RUNBOOK.md` | registry and package lifecycle clients |
| Styio / Contracts | `STYIO-CONTRACTS-RUNBOOK.md` | compiler and machine handoff |
| Docs / Delivery | `DOCS-DELIVERY-RUNBOOK.md` | SSOT, gates, and release evidence |

## Review Matrix

Public commands and metadata require Core and Docs review. Compile-plan changes
require Core and Styio review. Registry route or request changes require
Registry and Platform review. Cross-owner changes require fixed revisions.

## Escalation Rules

Implementation conflicts escalate to the CLI contract or accepted ADR. Service
behavior escalates to Platform. Compiler behavior escalates to Styio. No owner
conflict is resolved by copying the foreign implementation into Pafio.

## Checkpoint Policy

Close one independently testable capability or scenario at a time. Run focused
tests during implementation and one full regression after all closures.

## Release / Cutover Gates

Pafio, Styio, Platform, Vityo, site, audit, and aggregate revisions are pinned
before the coordinated nightly cutover. Any failed owner contract blocks the
cutover without introducing compatibility code.

## Handoff / Recovery

Record only revision, contract, status, and safe test evidence. Exclude
credentials, personal data, machine paths, and backend runtime payloads.
