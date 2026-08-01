#!/usr/bin/env python3
"""Validate immutable revisions in the Pafio ecosystem owner matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MATRIX = ROOT / "contracts" / "ecosystem" / "owner-matrix.json"
DEFAULT_REPOSITORIES_ROOT = ROOT.parent
ACCEPTANCE = ROOT / "tests" / "interop" / "ecosystem-owner-contract.py"

EXPECTED_CONTRACTS = {
    "compile_plan": ("pafio-nightly", 1),
    "diagnostics": ("styio-nightly", 1),
    "hosted_workspace": ("styio-cloud-nightly", 1),
    "machine_info": ("styio-nightly", 1),
    "metadata": ("pafio-nightly", 1),
    "receipt": ("styio-nightly", 1),
    "registry_control": ("styio-cloud-nightly", 1),
    "resolution": ("pafio-nightly", 1),
    "runtime_events": ("styio-nightly", 1),
    "workflow": ("pafio-nightly", 1),
}

EXPECTED_REPOSITORIES = {
    "pafio-nightly": "project-workflow",
    "styio-nightly": "compiler",
    "styio-cloud-nightly": "platform",
    "vityo-nightly": "consumer",
    "styio.io": "public-site",
    "styio-audit": "audit-policy",
    "styio": "release-aggregate",
}

EXPECTED_CHECK_IDS = {
    "audit-pafio-scope",
    "pafio-contract-versions",
    "pafio-no-legacy-compiler-profile",
    "platform-hosted-owner",
    "platform-registry-control-route",
    "platform-static-registry",
    "platform-worker-command",
    "site-pafio-entry",
    "styio-accepts-pafio-plan",
    "styio-no-build-mode",
    "umbrella-pafio-entry",
    "vityo-compiler-adapter",
    "vityo-hosted-adapter",
    "vityo-metadata-adapter",
    "vityo-no-private-home",
}

FULL_OBJECT_ID = re.compile(r"[0-9a-f]{40}|[0-9a-f]{64}")
METADATA_V1_FIELDS = {
    "package",
    "workspace",
    "dependencies",
    "targets",
    "lock",
    "resolution",
    "vendor",
}


class VerificationError(RuntimeError):
    """Raised when the owner matrix or a pinned revision is invalid."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise VerificationError(message)


def require_object(value: Any, context: str) -> dict[str, Any]:
    require(isinstance(value, dict), f"{context} must be an object")
    return value


def require_array(value: Any, context: str) -> list[Any]:
    require(isinstance(value, list), f"{context} must be an array")
    return value


def require_string(value: Any, context: str) -> str:
    require(isinstance(value, str) and value != "", f"{context} must be a non-empty string")
    return value


def safe_relative_path(value: Any, context: str, *, allow_tree: bool = False) -> str:
    path = require_string(value, context)
    if allow_tree and path == ".":
        return path
    pure = PurePosixPath(path)
    require(
        not pure.is_absolute()
        and "\\" not in path
        and path not in {".", ".."}
        and all(part not in {"", ".", ".."} for part in pure.parts),
        f"{context} is not a safe repository-relative path",
    )
    return path


def run_git(repo: Path, arguments: list[str], *, allow_no_match: bool = False) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), *arguments],
        text=True,
        capture_output=True,
        check=False,
    )
    if allow_no_match and result.returncode == 1:
        return ""
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "git object read failed"
        raise VerificationError(detail)
    return result.stdout


def read_blob(repo: Path, revision: str, path: str) -> str:
    return run_git(repo, ["show", f"{revision}:{path}"])


def path_exists(repo: Path, revision: str, path: str) -> bool:
    result = subprocess.run(
        ["git", "-C", str(repo), "cat-file", "-e", f"{revision}:{path}"],
        text=True,
        capture_output=True,
        check=False,
    )
    return result.returncode == 0


def tree_contains(repo: Path, revision: str, value: str) -> bool:
    result = subprocess.run(
        ["git", "-C", str(repo), "grep", "-F", "--quiet", "-e", value, revision, "--", "."],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode not in {0, 1}:
        detail = result.stderr.strip() or "git object search failed"
        raise VerificationError(detail)
    return result.returncode == 0


def validate_contracts(raw: Any) -> int:
    contracts = require_object(raw, "contracts")
    require(
        set(contracts) == set(EXPECTED_CONTRACTS),
        "contracts must define exactly: " + ", ".join(sorted(EXPECTED_CONTRACTS)),
    )
    for name, (owner, version) in EXPECTED_CONTRACTS.items():
        contract = require_object(contracts[name], f"contracts.{name}")
        require(contract.get("owner") == owner, f"contracts.{name}.owner must equal {owner}")
        require(contract.get("version") == version, f"contracts.{name}.version must equal {version}")

    require(
        contracts["compile_plan"].get("consumer") == "styio-nightly",
        "contracts.compile_plan.consumer must equal styio-nightly",
    )
    require(
        contracts["metadata"].get("consumer") == "vityo-nightly",
        "contracts.metadata.consumer must equal vityo-nightly",
    )
    require(
        contracts["machine_info"].get("consumer") == "vityo-nightly",
        "contracts.machine_info.consumer must equal vityo-nightly",
    )
    require(
        contracts["hosted_workspace"].get("consumer") == "vityo-nightly",
        "contracts.hosted_workspace.consumer must equal vityo-nightly",
    )
    registry = contracts["registry_control"]
    require(
        registry.get("protocol") == "pafio-static-registry",
        "contracts.registry_control.protocol must equal pafio-static-registry",
    )
    require(
        registry.get("base_path") == "/api/pafio-registry-control/v1",
        "contracts.registry_control.base_path must equal /api/pafio-registry-control/v1",
    )
    return len(contracts)


def validate_check(
    *,
    repo: Path,
    revision: str,
    raw: Any,
    kind: str,
    seen_ids: set[str],
) -> None:
    check = require_object(raw, f"repository {kind} check")
    check_id = require_string(check.get("id"), f"repository {kind} check id")
    require(check_id not in seen_ids, f"duplicate semantic check id: {check_id}")
    seen_ids.add(check_id)
    path = safe_relative_path(
        check.get("path"),
        f"semantic check {check_id} path",
        allow_tree=kind == "forbids",
    )
    values = require_array(check.get("values"), f"semantic check {check_id} values")
    require(values, f"semantic check {check_id} values must not be empty")
    checked_values = [
        require_string(value, f"semantic check {check_id} value") for value in values
    ]

    if path == ".":
        require(kind == "forbids", f"semantic check {check_id} may not read the whole tree")
        for value in checked_values:
            require(
                not tree_contains(repo, revision, value),
                f"{check_id}: forbidden value is present",
            )
        return

    require(
        path_exists(repo, revision, path),
        f"{check_id}: repository path is missing",
    )
    content = read_blob(repo, revision, path)
    if kind == "contains":
        for value in checked_values:
            require(value in content, f"{check_id}: required value is missing")
    else:
        for value in checked_values:
            require(value not in content, f"{check_id}: forbidden value is present")


def validate_repositories(raw: Any, repositories_root: Path) -> tuple[int, int, dict[str, str]]:
    entries = require_array(raw, "repositories")
    by_name: dict[str, dict[str, Any]] = {}
    for raw_entry in entries:
        entry = require_object(raw_entry, "repository entry")
        name = require_string(entry.get("name"), "repository name")
        require(name not in by_name, f"duplicate repository entry: {name}")
        by_name[name] = entry

    require(
        set(by_name) == set(EXPECTED_REPOSITORIES),
        "repositories must define exactly: " + ", ".join(sorted(EXPECTED_REPOSITORIES)),
    )

    seen_ids: set[str] = set()
    revisions: dict[str, str] = {}
    for name, expected_role in EXPECTED_REPOSITORIES.items():
        entry = by_name[name]
        require(entry.get("role") == expected_role, f"repository {name} role must equal {expected_role}")
        revision = require_string(entry.get("revision"), f"repository {name} revision")
        require(
            FULL_OBJECT_ID.fullmatch(revision) is not None,
            f"repository {name} revision must be a full immutable git object id",
        )

        repo = repositories_root / name
        require(repo.is_dir(), f"repository {name} is unavailable")
        resolved = run_git(repo, ["rev-parse", "--verify", f"{revision}^{{commit}}"]).strip()
        require(resolved == revision, f"repository {name} revision is not an immutable commit")
        revisions[name] = revision

        required_paths = require_array(
            entry.get("required_paths"), f"repository {name} required_paths"
        )
        require(required_paths, f"repository {name} required_paths must not be empty")
        for raw_path in required_paths:
            path = safe_relative_path(raw_path, f"repository {name} required path")
            require(
                path_exists(repo, revision, path),
                f"repository {name} required path is missing",
            )

        for raw_check in require_array(entry.get("contains"), f"repository {name} contains"):
            validate_check(
                repo=repo,
                revision=revision,
                raw=raw_check,
                kind="contains",
                seen_ids=seen_ids,
            )
        for raw_check in require_array(entry.get("forbids"), f"repository {name} forbids"):
            validate_check(
                repo=repo,
                revision=revision,
                raw=raw_check,
                kind="forbids",
                seen_ids=seen_ids,
            )

    require(
        seen_ids == EXPECTED_CHECK_IDS,
        "semantic checks must define exactly: " + ", ".join(sorted(EXPECTED_CHECK_IDS)),
    )
    return len(entries), len(seen_ids), revisions


def run_self_acceptance() -> None:
    require(ACCEPTANCE.is_file(), "ecosystem owner acceptance is missing")
    env = os.environ.copy()
    env["ECOSYSTEM_ACCEPTANCE_CHILD"] = "1"
    result = subprocess.run(
        [sys.executable, str(ACCEPTANCE)],
        cwd=ROOT,
        env=env,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "acceptance failed"
        raise VerificationError(f"self acceptance failed: {detail}")


def verify(
    matrix_path: Path,
    repositories_root: Path,
    *,
    mode: str = "focused",
) -> dict[str, Any]:
    try:
        encoded = matrix_path.read_bytes()
    except OSError as exc:
        raise VerificationError("owner matrix cannot be read") from exc
    try:
        matrix = json.loads(encoded)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise VerificationError("owner matrix is not valid UTF-8 JSON") from exc

    root = require_object(matrix, "owner matrix")
    require(root.get("schema_version") == 1, "schema_version must equal 1")
    contract_count = validate_contracts(root.get("contracts"))
    repository_count, check_count, revisions = validate_repositories(
        root.get("repositories"), repositories_root
    )
    return {
        "checks": check_count,
        "contracts": contract_count,
        "matrix_sha256": hashlib.sha256(encoded).hexdigest(),
        "mode": mode,
        "ok": True,
        "repositories": repository_count,
        "revisions": revisions,
        "schema_version": 1,
    }


def run_process(
    argv: list[str],
    *,
    cwd: Path,
    environment: dict[str, str],
    label: str,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        argv,
        cwd=cwd,
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )
    require(result.returncode == 0, f"{label} failed")
    return result


def parse_json_output(result: subprocess.CompletedProcess[str], label: str) -> dict[str, Any]:
    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        raise VerificationError(f"{label} did not emit one JSON object") from exc
    return require_object(payload, f"{label} output")


def file_sha256(path: Path, label: str) -> str:
    require(path.is_file(), f"{label} is missing")
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError as exc:
        raise VerificationError(f"{label} cannot be read") from exc


def require_full_input(path: Path | None, label: str) -> Path:
    require(path is not None and path.is_file(), f"{label} is required for --full")
    return path.resolve()


def run_full_product_matrix(
    *,
    payload: dict[str, Any],
    pafio_bin: Path,
    styio_bin: Path,
    vityo_root: Path,
    platform: str,
) -> None:
    revisions = require_object(payload.get("revisions"), "validated revisions")
    require(vityo_root.is_dir(), "Vityo repository is required for --full")
    vityo_revision = run_git(vityo_root, ["rev-parse", "HEAD"]).strip()
    require(
        vityo_revision == revisions.get("vityo-nightly"),
        "Vityo worktree does not match the fixed owner-matrix revision",
    )
    require(
        run_git(
            vityo_root,
            ["status", "--porcelain=v1", "--untracked-files=no"],
        ).strip()
        == "",
        "Vityo worktree has tracked changes",
    )
    product_gate = vityo_root / "scripts" / "ecosystem-product-gate.py"
    require(product_gate.is_file(), "Vityo ecosystem product gate is missing")

    with tempfile.TemporaryDirectory(prefix="pafio_release_matrix_") as temp_name:
        temp_root = Path(temp_name)
        environment = os.environ.copy()
        environment["PAFIO_HOME"] = str(temp_root / "pafio-home")
        environment.pop("PAFIO_STYIO_BIN", None)

        pafio_info = parse_json_output(
            run_process(
                [str(pafio_bin), "machine-info", "--json"],
                cwd=ROOT,
                environment=environment,
                label="Pafio machine-info",
            ),
            "Pafio machine-info",
        )
        require(pafio_info.get("tool") == "pafio", "Pafio machine-info tool identity is invalid")

        styio_info = parse_json_output(
            run_process(
                [str(styio_bin), "--machine-info=json"],
                cwd=ROOT,
                environment=environment,
                label="Styio machine-info",
            ),
            "Styio machine-info",
        )
        require(styio_info.get("tool") == "styio", "Styio machine-info tool identity is invalid")
        feature_flags = require_object(
            styio_info.get("feature_flags"), "Styio machine-info feature_flags"
        )
        require(
            feature_flags.get("compile_plan_consumer") is True,
            "Styio does not advertise compile-plan consumption",
        )

        project_root = temp_root / "project"
        run_process(
            [
                str(pafio_bin),
                "new",
                "pafio/release-matrix",
                str(project_root),
                "--bin",
            ],
            cwd=ROOT,
            environment=environment,
            label="Pafio new",
        )
        manifest_path = project_root / "pafio.toml"
        require(manifest_path.is_file(), "Pafio new did not create pafio.toml")

        cold_build = parse_json_output(
            run_process(
                [
                    str(pafio_bin),
                    "--json",
                    "build",
                    "--manifest-path",
                    str(manifest_path),
                    "--styio-bin",
                    str(styio_bin),
                ],
                cwd=ROOT,
                environment=environment,
                label="cold Pafio build",
            ),
            "cold Pafio build",
        )
        require(
            cold_build.get("action") == "build"
            and cold_build.get("status") == "succeeded",
            "cold Pafio build workflow envelope is invalid",
        )

        lock_path = project_root / "pafio.lock"
        resolution_path = project_root / ".pafio" / "resolution-v1.json"
        lock_digest = file_sha256(lock_path, "pafio.lock")
        resolution_digest = file_sha256(
            resolution_path, "resolution-v1.json"
        )

        frozen_build = parse_json_output(
            run_process(
                [
                    str(pafio_bin),
                    "--json",
                    "build",
                    "--manifest-path",
                    str(manifest_path),
                    "--styio-bin",
                    str(styio_bin),
                    "--frozen",
                ],
                cwd=ROOT,
                environment=environment,
                label="frozen Pafio build",
            ),
            "frozen Pafio build",
        )
        require(
            frozen_build.get("action") == "build"
            and frozen_build.get("status") == "succeeded",
            "frozen Pafio build workflow envelope is invalid",
        )
        require(
            file_sha256(lock_path, "pafio.lock") == lock_digest
            and file_sha256(resolution_path, "resolution-v1.json")
            == resolution_digest,
            "frozen Pafio build changed lock or resolution state",
        )

        metadata = parse_json_output(
            run_process(
                [
                    str(pafio_bin),
                    "metadata",
                    "--json",
                    "--manifest-path",
                    str(manifest_path),
                    "--frozen",
                ],
                cwd=ROOT,
                environment=environment,
                label="Pafio metadata v1",
            ),
            "Pafio metadata v1",
        )
        require(
            set(metadata) == METADATA_V1_FIELDS,
            "Pafio metadata v1 top-level fields are invalid",
        )

        vityo_result = run_process(
            [
                sys.executable,
                str(product_gate),
                "--require-real-matrix",
                "--platform",
                platform,
                "--pafio-bin",
                str(pafio_bin),
                "--styio-bin",
                str(styio_bin),
                "--json",
            ],
            cwd=vityo_root,
            environment=environment,
            label="Vityo owner-adapter product gate",
        )
        vityo_payload = parse_json_output(
            vityo_result, "Vityo owner-adapter product gate"
        )
        report = require_object(
            vityo_payload.get("report"), "Vityo owner-adapter report"
        )
        scenario_count = report.get("scenario_count")
        require(
            vityo_payload.get("ok") is True
            and isinstance(scenario_count, int)
            and scenario_count > 0,
            "Vityo owner-adapter product gate reported no accepted scenario",
        )

    payload["product_matrix"] = {
        "cold_build": True,
        "frozen_build": True,
        "lock_and_resolution_stable": True,
        "metadata_fields": sorted(METADATA_V1_FIELDS),
        "platform": platform,
        "styio_compile_plan_consumer": True,
        "vityo_scenarios": scenario_count,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Replay the Pafio ecosystem owner contract from immutable git objects."
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--focused",
        action="store_true",
        help="run the focused immutable owner-contract verification",
    )
    mode.add_argument(
        "--full",
        action="store_true",
        help="also execute the local Pafio/Styio/Vityo product matrix",
    )
    parser.add_argument("--matrix", type=Path, default=DEFAULT_MATRIX)
    parser.add_argument("--repositories-root", type=Path, default=DEFAULT_REPOSITORIES_ROOT)
    parser.add_argument("--pafio-bin", type=Path)
    parser.add_argument("--styio-bin", type=Path)
    parser.add_argument("--vityo-root", type=Path)
    parser.add_argument(
        "--platform",
        choices=("linux", "windows", "macos"),
        default={"darwin": "macos", "linux": "linux", "win32": "windows"}.get(
            sys.platform
        ),
    )
    args = parser.parse_args()

    if os.environ.get("ECOSYSTEM_ACCEPTANCE_CHILD") != "1":
        run_self_acceptance()
    selected_mode = "full" if args.full else "focused"
    payload = verify(
        args.matrix,
        args.repositories_root,
        mode=selected_mode,
    )
    if args.full:
        require(args.platform is not None, "current platform is unsupported")
        run_full_product_matrix(
            payload=payload,
            pafio_bin=require_full_input(args.pafio_bin, "--pafio-bin"),
            styio_bin=require_full_input(args.styio_bin, "--styio-bin"),
            vityo_root=(
                args.vityo_root.resolve()
                if args.vityo_root is not None
                else args.repositories_root / "vityo-nightly"
            ),
            platform=args.platform,
        )
    print(json.dumps(payload, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except VerificationError as exc:
        print(f"ecosystem-contracts: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
