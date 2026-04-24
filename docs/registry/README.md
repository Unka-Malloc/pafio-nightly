# Registry Docs

**Purpose:** Separate local package-manager registry client and offline package rules from server-side package distribution and registry control-plane ownership now moved to `styio-platform`.

**Last updated:** 2026-04-24

## Scope

- registry client behavior
- offline package cache and import/export expectations
- server/write behavior as compatibility references only
- deployment and operational baseline handoff to `styio-platform`
- industrial `v2` static read-plane protocol and publish-control-plane split

## Ownership

- static read-plane layout and object format remain owned by [Spio-Registry-V2-Protocol.md](./Spio-Registry-V2-Protocol.md)
- security-sensitive auth/account/trust boundaries live in [../security/Spio-Private-Security-Module-Contract.md](../security/Spio-Private-Security-Module-Contract.md)
- registry HTTP control-plane contract is retained here as a client compatibility reference and is owned operationally by `styio-platform`
- industrial `v2` read-plane rules live in [Spio-Registry-V2-Protocol.md](./Spio-Registry-V2-Protocol.md)
- industrial `v2` publish-plane responsibilities live in [Spio-Registry-V2-Publish-Control-Plane.md](./Spio-Registry-V2-Publish-Control-Plane.md)
- client-side consumption rules live in [Spio-Registry-Client-Contract.md](./Spio-Registry-Client-Contract.md)
- deployment baseline lives in [Spio-Registry-Deployment-Baseline.md](./Spio-Registry-Deployment-Baseline.md)
- executable server validation steps move to `styio-platform`; this repo keeps package-manager fetch/publish client gates
- global package distribution, regional mirrors, and mirror synchronization are platform-owned; local offline packages remain client-owned

## Maintenance Rule

- do not redefine the shared blob/index layout here
- do not mix client cache rules into server deployment checklists
- do not mix server upload/auth policy into client fetch semantics
- do not place private credential, allowlist, or account policy details under this tracked directory
- keep registry distribution `v2`-only in active contracts
- keep static read-plane contracts and HTTP control-plane contracts distinct; clients and service operators consume different surfaces
- keep local import/export and offline cache semantics independent from platform mirror availability
