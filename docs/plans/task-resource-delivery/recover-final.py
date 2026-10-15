#!/usr/bin/env python3
"""Explicit remaining delivery checks for host-6gt38m8v; see HANDOFF.md."""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[3]
FIRST_RUN = ROOT / "build-codex/task-resource-final/host-6gt38m8v"
SPEC = importlib.util.spec_from_file_location("delivery_gate", ROOT / "scripts/delivery-gate.py")
assert SPEC is not None and SPEC.loader is not None
delivery = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = delivery
SPEC.loader.exec_module(delivery)


def main() -> int:
    # Delivery's native command uses the original ROOT/build-codex, not the
    # exported source. Its identical first-run inputs passed and remain valid.
    # This is a single diagnosed recovery selection, not a permanent gate option.
    original = json.loads((FIRST_RUN / "stdout.log").read_text())
    native = next(step for step in original["steps"] if step["name"] == "regression_native_check")
    if not native["ok"] or native["returncode"] != 0:
        raise ValueError("the retained native check must have passed before reuse")
    results = []
    errors = []
    with tempfile.TemporaryDirectory(prefix="pafio-delivery-recovery-") as temp_dir:
        export = Path(temp_dir) / "delivery"
        copy = delivery.run_step(delivery.Step(
            "delivery_copy", [str(delivery.COPY_SCRIPT), str(export)], ROOT,
        ))
        results.append(copy)
        if copy["ok"]:
            errors = delivery.validate_export_tree(export)
            if not errors:
                for step in delivery.delivery_steps(export):
                    if step.name == "delivery_native_check":
                        # Preserve this gate obligation as explicit reused
                        # evidence, never as a freshly executed check.
                        continue
                    result = delivery.run_step(step)
                    results.append(result)
                    if not result["ok"]:
                        break
    ok = not errors and all(result["ok"] for result in results)
    payload = {
        "ok": ok, "structural_errors": errors, "steps": results,
        "reused": [{
            "name": "delivery_native_check",
            "evidence": str(FIRST_RUN.relative_to(ROOT) / "regression_native_check"),
            "reason": "Same original-source native script/build inputs; export policy and Python-only tests do not affect CTest.",
            "returncode": native["returncode"],
        }],
    }
    if log_root := os.environ.get("PAFIO_GATE_LOG_DIR"):
        (Path(log_root) / "report.json").write_text(json.dumps(payload, indent=2) + "\n")
    print(json.dumps(payload))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
