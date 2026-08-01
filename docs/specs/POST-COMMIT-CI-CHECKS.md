# Post-Commit CI Checks

**Purpose:** Define the required workflow for checking GitHub Actions after a local commit is pushed, including what must be verified before committing and what must be watched after pushing.

**Last updated:** 2026-08-01

## Scope

This spec applies to agent and maintainer work on `pafio-nightly` branches. It covers local pre-commit verification, post-push GitHub Actions monitoring, and failure recovery for repository-local and cross-repository gates.

## Commit-Time Verification

Before creating a commit, the agent must run the closest local equivalent of the GitHub Actions checks affected by the change.

Minimum local checks for normal changes:

```bash
ctest --test-dir build-codex --output-on-failure
python3 scripts/repo-hygiene-gate.py --mode tracked
python3 scripts/docs-audit.py
```

The GitHub Actions CI floor is the repository-local `local-ci-gate` workflow.
It owns the range-aware docs/repo hygiene checks, native submit gate,
extractability gate, performance smoke gate, and delivery package gate that
were previously split across `styio-ci`, `repo-hygiene`, and `Submit Gate`.
`local-ci-gate` is the pafio repository's own CI surface; it is not the shared
Styio ecosystem resource gate modeled by upstream `styio-ci-gate`.
Inside that workflow, `Linux / Debian 13 trixie gate` is the blocking repository
gate for shared-source `nightly` promotion. `macOS / latest native smoke` is an
independent platform adaptation lane. `Windows / latest native smoke` runs only
through explicit `workflow_dispatch`; Windows release owners execute it from
the Windows release flow and must resolve any failure before publishing Windows
artifacts. Neither a shared-source merge nor another platform's passing result
constitutes Windows release evidence.

For cross-repository checkouts, CI first tries the Pafio branch name and falls
back independently to each sibling repository's `nightly` branch when that
branch does not exist there. A Pafio-only change therefore does not require
creating empty matching branches in Styio or Vityo.

Cross-repository contract or product changes must replay the immutable owner
matrix and the matching executable product gate:

```bash
cd <pafio-workspace>
python3 scripts/ecosystem-cli-doc-gate.py --require-workspace --workspace-root <workspace-root>

python3 scripts/verify-ecosystem-contracts.py --focused --repositories-root <owner-repositories>
python3 scripts/verify-ecosystem-contracts.py --full --repositories-root <owner-repositories> --pafio-bin <pafio> --styio-bin <styio> --vityo-root <vityo-workspace>
```

The docs gate validates the Pafio-owned surface locally by default. Its explicit
workspace form checks fixed Pafio, Styio, Styio Platform, and Vityo
documentation; it never discovers or delegates to a mutable sibling checkout.
The focused replay reads only the commits pinned by
`contracts/ecosystem/owner-matrix.json`; dirty or newer sibling worktrees do not
change its result. The full gate additionally proves public `pafio new`, cold
automatic sync/build, stable frozen build state, metadata v1, system Styio
compile-plan consumption, and the Vityo owner adapters. The commit message body
should record the checks that were actually run.

## Post-Push Verification

After pushing a commit, the agent must actively check GitHub Actions while the current work turn remains open.

Required steps:

1. Resolve the current branch and pushed commit.
2. Query GitHub Actions for the repository and branch.
3. Watch the relevant workflow run or check suite until it reaches a terminal state when the expected runtime is reasonable.
4. If a check fails, inspect the failing job logs, identify the smallest fix, run the matching local gate, create a follow-up commit, and push again.
5. If a check is still queued or running when the turn must end, report the run URL, current status, and the command needed to resume checking.

Preferred commands:

```bash
gh run list --branch "$(git branch --show-current)" --limit 10
gh run watch <run-id> --exit-status
gh run view <run-id> --log-failed
```

If `gh` is unavailable or unauthenticated, the agent must state that GitHub Actions could not be checked directly and include the local gates that were run instead.

## Cross-Repository Work

When one delivery touches `styio-nightly`, `pafio-nightly`,
`styio-cloud-nightly`, and `vityo-nightly`, post-push verification applies to
every pushed repository. The agent should check each repository's GitHub
Actions status, not only the repository that received the last commit.

Cross-repository gates must use the same workspace checkout set that will be visible to CI. If a gate consumes another repository's branch, push that repository first or report that remote CI may still be using an older sibling checkout.

## Delivery Ruleset Governance

Required GitHub merge gates are maintained through GitHub Rulesets, not legacy classic branch protection. Protected downstream branches should require `audit` and the blocking Linux `local-ci-gate` job as the stable repository-local status-check surface, with `styio-audit` kept as the policy workflow check where the upstream ruleset expects it. macOS results apply only to macOS adaptation. Windows is a separately dispatched release lane and must not become a shared-source merge requirement or inherit another platform's acceptance evidence.

Gate audits must inspect effective branch rules, for example:

```bash
gh api repos/Unka-Malloc/pafio-nightly/rules/branches/ai-dev
```

Do not use `branches/ai-dev/protection/required_status_checks` as the authority for this repository. That legacy classic endpoint can return 404 even when the Ruleset gate is active.

## Completion Criteria

A pushed change is not complete until one of these is true:

1. GitHub Actions checks passed.
2. GitHub Actions checks failed, the failure was fixed and re-pushed, and the replacement run passed.
3. GitHub Actions could not be observed within the current turn, and the final handoff records the unresolved run status and recovery command.
