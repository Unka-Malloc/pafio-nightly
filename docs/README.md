# spio Docs

**Purpose:** Define the modular structure of `spio/docs/` so planning, policy, operations, and external compiler knowledge can evolve without drifting into one another.

**Last updated:** 2026-06-28

## Entry Points

1. Build and dev environment: [BUILD-AND-DEV-ENV.md](./BUILD-AND-DEV-ENV.md)
2. Plan workspace: [plan/README.md](./plan/README.md)
3. Active package-manager plans: [plan/README.md](./plan/README.md)
4. Version-decoupling rules: [governance/Spio-Version-Decoupling-Constraints.md](./governance/Spio-Version-Decoupling-Constraints.md)
5. Local offline package contract: [governance/Spio-Local-Offline-Package-Contract.md](./governance/Spio-Local-Offline-Package-Contract.md)
6. Platform and control-console split: [governance/Spio-Control-Console-And-Service-Split.md](./governance/Spio-Control-Console-And-Service-Split.md)
7. Package-manager verification matrix: [operations/Spio-Verification-Matrix.md](./operations/Spio-Verification-Matrix.md)
8. External compiler knowledge pack: [external/for-styio/Styio-for-Spio-Developers.md](./external/for-styio/Styio-for-Spio-Developers.md)

## Modules

- `assets/` — reusable workflow and gate documentation
- `governance/` — normative rules and single-source contracts
- `security/` — public/private security boundary for auth, account, and trust hooks
- `specs/` — cross-cutting agent, audit, repository-boundary, dependency, and documentation rules
- `adr/` — durable design and implementation decisions
- `plan/` — Better Plan workspace, active functional execution plans, and checkpoint graphs
- `registry/` — registry v2 static read-plane, package-manager client contracts, offline package expectations, and server control-plane handoff notes pointing to `styio-platform`
- `operations/` — gates, preflight, package-manager validation, and split runbooks
- `external/for-styio/` — external compiler knowledge pack and public interface expectations
- `teams/` — owner runbooks, review routing, and delivery-facing ownership boundaries

## Recommended Reading Order

1. `BUILD-AND-DEV-ENV.md`
2. `plan/README.md`
3. `plan/Manifest.json`
4. `plan/workflow-toolchain/README.md`
5. `governance/Spio-Version-Decoupling-Constraints.md`
6. `adr/INDEX.md`
7. `governance/Spio-Cloud-Control-Plane-Contract.md`
8. `governance/Spio-Entry-Argument-Index.md`
9. `registry/Spio-Registry-V2-Protocol.md`
10. `governance/Spio-Local-Offline-Package-Contract.md`
11. `registry/Spio-Registry-Control-Plane-Contract.md`
12. `registry/Spio-Registry-V2-Publish-Control-Plane.md`
13. `governance/Docs-Maintenance-Model.md`
14. `specs/audit/CODE-AUDIT-CHECKLIST.md`
15. `security/Spio-Private-Security-Module-Contract.md`
16. `registry/Spio-Registry-Client-Contract.md`
17. `registry/Spio-Registry-Deployment-Baseline.md`
18. `plan/delivery-quality/README.md`
19. `plan/spio-foundation/README.md`
20. `plan/linux-compatibility/README.md`
21. `plan/macos-compatibility/README.md`
22. `plan/windows-compatibility/README.md`
23. `operations/Spio-Verification-Matrix.md`
24. `governance/Spio-Control-Console-And-Service-Split.md`
25. `operations/Spio-Repo-Split-Runbook.md`
26. `external/for-styio/Styio-External-Interface-Requirement-Spec.md`
27. `external/for-styio/Styio-for-Spio-Developers.md`

## Precedence

When documents disagree:

1. `governance/`
2. `security/`
3. `registry/`
4. `specs/`
5. `adr/`
6. `operations/`
7. `plan/`
8. `external/for-styio/`

## Maintenance Rules

- Files here describe `spio` as if it were already an independent repository.
- Do not duplicate a rule across modules; link to the owner document instead.
- Keep named gate commands in `operations/Spio-Verification-Matrix.md`.
- Keep implemented decisions in `adr/`; do not leave completed behavior documented only as planning or history.
- Repository-level docs automation entrypoints are `scripts/docs-index.py`, `scripts/docs-lifecycle.py`, and `scripts/docs-audit.py`.
- When a rule here conflicts with an implementation shortcut, the shortcut must be treated as technical debt and recorded explicitly.
