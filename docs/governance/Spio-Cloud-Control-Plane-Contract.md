# Spio Cloud Control-Plane Contract

**Purpose:** Define the baseline machine contract for `spio` cloud execution policy, worker-pool routing keys, and project-local execution preferences without pretending the tracked open-source tree already ships the full remote control plane.

**Last updated:** 2026-04-20

## Scope

This document owns:

- cloud execution policy terminology used by the native CLI
- the project-local cloud preference fields recorded in `spio-toolchain.lock`
- the JSON shape exposed through `spio cloud status --json`
- the machine-readable cloud policy payload embedded in workflow success JSON
- the worker-pool key dimensions that future remote execution must preserve

This document does not own:

- registry repository layout
- publish/fetch path contracts
- external `styio` binary compatibility
- full remote API routes for a future cloud control plane

Those remain in the existing governance, registry, and `styio` contract documents.

## Baseline Position

The tracked open-source native core currently exposes a **local cloud-execution baseline**. It does **not** yet implement a production multi-service control plane, queue, worker pool, or remote scheduler.

What is implemented today:

- project-local persistence of cloud execution preferences in `spio-toolchain.lock`
- deterministic policy resolution from toolchain mode, channel, build mode, risk class, preferred execution lane, and security profile
- machine-readable introspection through `spio machine-info --json` and `spio cloud status --json`
- workflow success payloads that surface the resolved cloud execution policy

The purpose of this baseline is to freeze the terminology and policy surface before remote execution is introduced.

## Terms

### Execution Lane

`ExecutionLane` currently accepts:

- `isolated`
- `warm-shared`

Normative rules:

- `isolated` is the default lane
- `warm-shared` is opt-in and may be downgraded by policy resolution
- callers must treat the resolved lane, not the preferred lane, as authoritative

### Risk Class

`RiskClass` currently accepts:

- `trusted-internal`
- `partner-controlled`
- `untrusted-user`

Normative rules:

- `untrusted-user` is the default
- `untrusted-user` must resolve to `isolated`
- `partner-controlled` may request `warm-shared`, but the open-source baseline resolves it back to `isolated`
- only `trusted-internal` may currently keep `warm-shared`

### Security Profile

`SecurityProfile` currently accepts:

- `sandbox-default`
- `partner-restricted`
- `trusted-warm`

Normative rules:

- `sandbox-default` is the default profile
- the resolved security profile follows the resolved lane and risk class
- callers may not assume the project-local preferred profile remains unchanged after policy resolution

## Project-Local State

`spio-toolchain.lock` remains the project-local source of truth for execution preferences.

Required persisted fields:

- `[toolchain]`
  - `mode = "binary" | "build"`
  - `channel = "stable" | "nightly"`
  - `build = "minimal"`
- `[cloud]`
  - `risk = "trusted-internal" | "partner-controlled" | "untrusted-user"`
  - `lane = "isolated" | "warm-shared"`
  - `security = "sandbox-default" | "partner-restricted" | "trusted-warm"`

Defaults:

- `mode = "binary"`
- `channel = "stable"`
- `build = "minimal"`
- `risk = "untrusted-user"`
- `lane = "isolated"`
- `security = "sandbox-default"`

## Machine-Readable Introspection

### `spio machine-info --json`

The self-description endpoint must advertise:

- `supported_contracts.cloud_execution_policy = [1]`
- `supported_contracts.worker_pool_keys = [1]`

This only indicates that the local native core understands the cloud policy contract. It does **not** imply that a remote scheduler or distributed worker system is active.

### `spio cloud status --json`

Canonical form:

```text
spio cloud status --json [--manifest-path <path>]
```

This command must report at least:

- selected manifest path
- project-local `toolchain_mode`
- project-local `channel`
- project-local `build_mode`
- persisted `risk_class`
- persisted `preferred_execution_lane`
- persisted `security_profile`
- `supported_execution_lanes`
- `supported_risk_classes`
- `supported_security_profiles`
- resolved `cloud` policy object

## Resolved Cloud Policy

The resolved `cloud` object must include:

- `execution_lane`
- `risk_class`
- `security_profile`
- `worker_trust_tier`
- `cache_policy`
- `worker_pool_key`

### Cache Policy

`cache_policy` currently reports:

- `shared_layers_read_only`
- `worker_local_reuse`
- `shared_cache_promotion_eligible`

Normative baseline:

- shared layers are always read-only
- worker-local reuse is allowed only for `warm-shared`
- shared-cache promotion eligibility is currently true only for `trusted-internal`

### Worker Pool Key

The worker-pool key dimensions are:

- `platform`
- `arch`
- `toolchain_mode`
- `channel`
- `build_mode`
- `compiler_fingerprint`
- `base_image_revision`

These dimensions are frozen now so future remote worker pools do not invent incompatible routing keys later.

## Command Grammar Ownership

The cloud preference surface currently uses the same public grammar family as the rest of the project-local toolchain state:

- `spio use <binary|build>`
- `spio set channel as <stable|nightly>`
- `spio set build as minimal`
- `spio set risk as <trusted-internal|partner-controlled|untrusted-user>`
- `spio set lane as <isolated|warm-shared>`
- `spio set security as <sandbox-default|partner-restricted|trusted-warm>`

The parser may accept compact compatibility forms without `as`, but normative docs and help output must keep the `as` spelling.

## Relationship To Future Remote Execution

When the enterprise async control plane is introduced, it must preserve this document's:

- execution-lane vocabulary
- risk-class vocabulary
- security-profile vocabulary
- cache-policy semantics
- worker-pool key dimensions
- project-local state meanings

Remote APIs may extend these concepts, but they must not silently redefine them.
