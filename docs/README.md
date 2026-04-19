# spio Docs

**Purpose:** Define the modular structure of `spio/docs/` so planning, policy, operations, and external compiler knowledge can evolve without drifting into one another.

**Last updated:** 2026-04-19

## Entry Points

1. Build and dev environment: [BUILD-AND-DEV-ENV.md](./BUILD-AND-DEV-ENV.md)
2. Planning roadmap: [planning/Spio-Master-Plan.md](./planning/Spio-Master-Plan.md)
3. Stage review and future direction: [planning/Spio-Stage-Review-and-Future-Features.md](./planning/Spio-Stage-Review-and-Future-Features.md)
4. Version-decoupling rules: [governance/Spio-Version-Decoupling-Constraints.md](./governance/Spio-Version-Decoupling-Constraints.md)
5. Verification matrix: [operations/Spio-Verification-Matrix.md](./operations/Spio-Verification-Matrix.md)
6. External compiler knowledge pack: [styio/Styio-for-Spio-Developers.md](./styio/Styio-for-Spio-Developers.md)

## Modules

- `assets/` — reusable workflow and gate documentation
- `governance/` — normative rules and single-source contracts
- `security/` — public/private security boundary for auth, account, and trust hooks
- `adr/` — durable design and implementation decisions
- `planning/` — phases, workstreams, retrospectives, and TODO decomposition
- `registry/` — client contract, server contract, and deployment baseline for shared package distribution
- `operations/` — gates, preflight, registry server runbook, and split runbook
- `styio/` — external compiler knowledge pack and public interface expectations
- `teams/` — owner runbooks, review routing, and delivery-facing ownership boundaries

## Recommended Reading Order

1. `BUILD-AND-DEV-ENV.md`
2. `planning/Spio-Master-Plan.md`
3. `planning/Spio-Stage-Review-and-Future-Features.md`
4. `planning/Spio-Future-Direction-and-Styio-Coordination.md`
5. `governance/Spio-Version-Decoupling-Constraints.md`
6. `adr/INDEX.md`
7. `governance/Spio-Entry-Argument-Index.md`
8. `governance/Spio-Registry-Repository-Contract.md`
9. `governance/Docs-Maintenance-Model.md`
10. `security/Spio-Private-Security-Module-Contract.md`
11. `registry/Spio-Registry-Client-Contract.md`
12. `registry/Spio-Registry-Server-Contract.md`
13. `registry/Spio-Registry-Deployment-Baseline.md`
14. `planning/Spio-Workstreams-and-TODOs.md`
15. `operations/Spio-Verification-Matrix.md`
16. `operations/Spio-Registry-Server-Runbook.md`
17. `styio/Styio-External-Interface-Requirement-Spec.md`
18. `styio/Styio-for-Spio-Developers.md`
19. `operations/Spio-Repo-Split-Runbook.md`

## Precedence

When documents disagree:

1. `governance/`
2. `security/`
3. `registry/`
4. `adr/`
5. `operations/`
6. `planning/`
7. `styio/`

## Maintenance Rules

- Files here describe `spio` as if it were already an independent repository.
- Do not duplicate a rule across modules; link to the owner document instead.
- Keep named gate commands in `operations/Spio-Verification-Matrix.md`.
- Repository-level docs automation entrypoints are `scripts/docs-index.py`, `scripts/docs-lifecycle.py`, and `scripts/docs-audit.py`.
- When a rule here conflicts with an implementation shortcut, the shortcut must be treated as technical debt and recorded explicitly.
