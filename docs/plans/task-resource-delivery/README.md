# Task Resource Delivery

**Purpose:** Own the task/resource milestone state, evidence, and check surface for the active delivery programme.

**Last updated:** 2026-10-15

[Tree.json](Tree.json) is the milestone tree; `nodes/` and `tasks/` own the split
execution state. [HANDOFF.md](HANDOFF.md), [REPAIR.md](REPAIR.md), and
[BASELINE.md](BASELINE.md) record the candidate handoff, repair evidence, and
capability baseline. `checks.sh` is the executable check surface
(`baseline|focused|integration|final|review`); `retain-final.py` wraps
final-evidence retention.
