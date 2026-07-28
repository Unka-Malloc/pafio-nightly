# Resolver Offline Cache Plan

**Purpose:** Manage remaining work for deterministic resolution, lock updates, offline package use, vendoring, exact sync, cache maintenance, and content-addressed state.

**Last updated:** 2026-06-28

## State Files

- `../Manifest.json` indexes this plan.
- `Checkpoints.json` is the executable checkpoint graph for this functional area.

## Planning Rule

This plan is a convergence target for the current implementation. Nodes may be executed separately, but they must converge on one current package-manager behavior and one implementation path.
