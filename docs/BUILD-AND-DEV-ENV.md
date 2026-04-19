# spio Build And Dev Environment

**Purpose:** Provide the repository-level entry point for bootstrapping a fresh machine, configuring the native build, and finding the next operational docs.

**Last updated:** 2026-04-19

## Who This Is For

1. Contributors bringing up `spio` on a fresh Debian/Ubuntu VM or container.
2. Contributors who need the common native build, test, and preflight commands.
3. Contributors validating `spio` against an external `styio` binary through the public machine contract.

## Fresh Machine Bootstrap

From the repository root:

```bash
./scripts/bootstrap-dev-env.sh
```

That installs the native C++20 and Python tooling used by the repository on Debian/Ubuntu.

## Required Toolchains

1. A C++20 compiler.
2. CMake and Ninja or another supported generator.
3. Python 3 for preflight and verification scripts.
4. An external `styio` executable only when exercising compatibility and non-dry-run workflow handoff.

## Typical Build And Test Commands

Configure:

```bash
cmake -S . -B build -G Ninja
```

Build and run native tests:

```bash
cmake --build build
ctest --test-dir build
```

Run repository-native verification:

```bash
./scripts/checkpoint-health.sh
./scripts/delivery-gate.sh --mode checkpoint --skip-health
```

Run preflight against an external compiler:

```bash
./scripts/checkpoint-health.sh --styio-bin /absolute/path/to/styio
```

## Subsystem-Specific Follow-Ups

1. Planning and migration roadmap: [planning/Spio-Master-Plan.md](./planning/Spio-Master-Plan.md)
2. Verification matrix: [operations/Spio-Verification-Matrix.md](./operations/Spio-Verification-Matrix.md)
3. External compiler requirements: [styio/Styio-External-Interface-Requirement-Spec.md](./styio/Styio-External-Interface-Requirement-Spec.md)
4. Script inventory: [../scripts/README.md](../scripts/README.md)

## Related Docs

1. Docs tree guide: [README.md](./README.md)
2. Version-decoupling rules: [governance/Spio-Version-Decoupling-Constraints.md](./governance/Spio-Version-Decoupling-Constraints.md)
3. Repo split runbook: [operations/Spio-Repo-Split-Runbook.md](./operations/Spio-Repo-Split-Runbook.md)
