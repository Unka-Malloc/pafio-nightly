# spio Scripts

**Purpose:** Hold repository-local helper scripts used to validate extractability, contract hygiene, and black-box test setup for `spio`.

**Last updated:** 2026-04-19

## Current Public Scripts

For fresh-machine bootstrap and the common build/test flow, start with [../docs/BUILD-AND-DEV-ENV.md](../docs/BUILD-AND-DEV-ENV.md).

- docs index generator
- docs lifecycle validator
- docs/process gate
- repository hygiene gate
- team runbook maintenance gate
- checkpoint health gate
- delivery gate
- native configure/build/test entrypoint
- Debian/Ubuntu dev environment bootstrap
- extractability self-check
- contract fixture validation
- black-box integration runner setup
- compiler handoff and interface acceptance gate
- registry server publish/fetch acceptance gate
- registry promotion from writable origin storage to read-side serving root
- copy subtree into an external standalone repository target
- migration preflight that chains bootstrap, extractability, and optional external compiler handshake checks

## Private Script Rule

- auth-bearing registry publish smoke gates belong under the gitignored `scripts-private/` tree
- the tracked public scripts may validate redacted security metadata, but they must not carry credentials or deployment-owned auth policy
