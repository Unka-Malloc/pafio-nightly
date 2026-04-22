# Registry Docs

**Purpose:** Separate shared registry layout from client-side consumption rules, server-side write rules, and deployment guidance so `spio` can serve both client and server roles without mixing responsibilities.

**Last updated:** 2026-04-21

## Scope

- registry client behavior
- registry server/write behavior
- deployment and operational baseline for package distribution
- industrial `v2` static read-plane protocol and publish-control-plane split

## Ownership

- static read-plane layout and object format remain owned by [Spio-Registry-V2-Protocol.md](./Spio-Registry-V2-Protocol.md)
- security-sensitive auth/account/trust boundaries live in [../security/Spio-Private-Security-Module-Contract.md](../security/Spio-Private-Security-Module-Contract.md)
- registry HTTP control-plane contract lives in [Spio-Registry-Control-Plane-Contract.md](./Spio-Registry-Control-Plane-Contract.md)
- industrial `v2` read-plane rules live in [Spio-Registry-V2-Protocol.md](./Spio-Registry-V2-Protocol.md)
- industrial `v2` publish-plane responsibilities live in [Spio-Registry-V2-Publish-Control-Plane.md](./Spio-Registry-V2-Publish-Control-Plane.md)
- client-side consumption rules live in [Spio-Registry-Client-Contract.md](./Spio-Registry-Client-Contract.md)
- deployment baseline lives in [Spio-Registry-Deployment-Baseline.md](./Spio-Registry-Deployment-Baseline.md)
- executable server validation steps live in [../operations/Spio-Registry-Server-Runbook.md](../operations/Spio-Registry-Server-Runbook.md)

## Maintenance Rule

- do not redefine the shared blob/index layout here
- do not mix client cache rules into server deployment checklists
- do not mix server upload/auth policy into client fetch semantics
- do not place private credential, allowlist, or account policy details under this tracked directory
- keep registry distribution `v2`-only in active contracts
- keep static read-plane contracts and HTTP control-plane contracts distinct; clients and service operators consume different surfaces
