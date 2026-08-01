#!/usr/bin/env python3
"""Black-box acceptance for the fixed-revision ecosystem owner verifier."""

from __future__ import annotations

import copy
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
VERIFIER = ROOT / "scripts" / "verify-ecosystem-contracts.py"

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

FIXED_GIT_ENV = {
    "GIT_AUTHOR_DATE": "2026-07-30T00:00:00Z",
    "GIT_COMMITTER_DATE": "2026-07-30T00:00:00Z",
}


class AcceptanceFailure(RuntimeError):
    pass


def run(
    argv: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    return subprocess.run(
        argv,
        cwd=cwd,
        env=merged_env,
        text=True,
        capture_output=True,
        check=False,
    )


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AcceptanceFailure(message)


def git(repo: Path, *arguments: str) -> str:
    result = run(
        ["git", "-C", str(repo), *arguments],
        cwd=repo,
        env=FIXED_GIT_ENV,
    )
    require(
        result.returncode == 0,
        f"git {' '.join(arguments)} failed:\n{result.stderr}",
    )
    return result.stdout.strip()


def commit_files(repo: Path, files: dict[str, str], message: str) -> str:
    for relative, content in files.items():
        path = repo / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    git(repo, "add", "--all")
    git(repo, "commit", "-q", "-m", message)
    revision = git(repo, "rev-parse", "HEAD")
    require(
        re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", revision) is not None,
        f"fixture commit is not a full object id: {revision!r}",
    )
    return revision


def initialize_repository(root: Path, name: str, files: dict[str, str]) -> tuple[Path, str]:
    repo = root / name
    repo.mkdir(parents=True)
    result = run(["git", "init", "-q"], cwd=repo)
    require(result.returncode == 0, f"git init failed for {name}: {result.stderr}")
    git(repo, "config", "user.name", "Pafio Acceptance")
    git(repo, "config", "user.email", "acceptance@example.invalid")
    return repo, commit_files(repo, files, "accepted owner contract")


def fixture_sources() -> dict[str, dict[str, str]]:
    return {
        "pafio-nightly": {
            "contracts/ecosystem/pafio-owner.txt": "\n".join(
                [
                    "metadata v1",
                    "resolution v1",
                    "compile-plan v1",
                    "workflow v1",
                    "generated_by.tool=pafio",
                    "",
                ]
            )
        },
        "styio-nightly": {
            "tests/interop/pafio-compile-plan.txt": "\n".join(
                [
                    "compile-plan accepted",
                    "generated_by.tool=pafio",
                    "machine-info v1",
                    "diagnostics v1",
                    "receipt v1",
                    "runtime-events v1",
                    "",
                ]
            )
        },
        "styio-cloud-nightly": {
            "contracts/ecosystem/platform-owner.txt": "\n".join(
                [
                    "pafio-static-registry",
                    "/api/pafio-registry-control/v1",
                    "worker argv: pafio build",
                    "hosted-workspace v1",
                    "",
                ]
            )
        },
        "vityo-nightly": {
            "products/vityo_app/lib/owner_adapters/pafio_metadata_adapter.dart":
                "exec: pafio metadata --json\n",
            "products/vityo_app/lib/owner_adapters/styio_compiler_adapter.dart":
                "exec: styio --machine-info=json\n",
            "products/vityo_app/lib/owner_adapters/platform_hosted_adapter.dart":
                "consume: Platform hosted-workspace v1\n",
        },
        "styio.io": {
            "README.md": "Pafio is the project entry; Styio is system-provided.\n"
        },
        "styio-audit": {
            "README.md":
                "Audit Pafio metadata/workflow and Platform hosted/registry ownership.\n"
        },
        "styio": {
            "README.md":
                "Use Pafio for project workflows and system Styio for compilation.\n"
        },
    }


def contract_map() -> dict[str, dict[str, Any]]:
    contracts = {
        name: {"owner": owner, "version": version}
        for name, (owner, version) in EXPECTED_CONTRACTS.items()
    }
    contracts["compile_plan"]["consumer"] = "styio-nightly"
    contracts["metadata"]["consumer"] = "vityo-nightly"
    contracts["machine_info"]["consumer"] = "vityo-nightly"
    contracts["registry_control"].update(
        {
            "protocol": "pafio-static-registry",
            "base_path": "/api/pafio-registry-control/v1",
        }
    )
    contracts["hosted_workspace"]["consumer"] = "vityo-nightly"
    return contracts


def repository_entries(revisions: dict[str, str]) -> list[dict[str, Any]]:
    return [
        {
            "name": "pafio-nightly",
            "role": EXPECTED_REPOSITORIES["pafio-nightly"],
            "revision": revisions["pafio-nightly"],
            "required_paths": ["contracts/ecosystem/pafio-owner.txt"],
            "contains": [
                {
                    "id": "pafio-contract-versions",
                    "path": "contracts/ecosystem/pafio-owner.txt",
                    "values": [
                        "metadata v1",
                        "resolution v1",
                        "compile-plan v1",
                        "workflow v1",
                        "generated_by.tool=pafio",
                    ],
                }
            ],
            "forbids": [
                {
                    "id": "pafio-no-legacy-compiler-profile",
                    "path": "contracts/ecosystem/pafio-owner.txt",
                    "values": ["build_mode", '"bootstrap": true'],
                }
            ],
        },
        {
            "name": "styio-nightly",
            "role": EXPECTED_REPOSITORIES["styio-nightly"],
            "revision": revisions["styio-nightly"],
            "required_paths": ["tests/interop/pafio-compile-plan.txt"],
            "contains": [
                {
                    "id": "styio-accepts-pafio-plan",
                    "path": "tests/interop/pafio-compile-plan.txt",
                    "values": [
                        "compile-plan accepted",
                        "generated_by.tool=pafio",
                        "machine-info v1",
                        "diagnostics v1",
                        "receipt v1",
                        "runtime-events v1",
                    ],
                }
            ],
            "forbids": [
                {
                    "id": "styio-no-build-mode",
                    "path": "tests/interop/pafio-compile-plan.txt",
                    "values": ["build_mode"],
                }
            ],
        },
        {
            "name": "styio-cloud-nightly",
            "role": EXPECTED_REPOSITORIES["styio-cloud-nightly"],
            "revision": revisions["styio-cloud-nightly"],
            "required_paths": ["contracts/ecosystem/platform-owner.txt"],
            "contains": [
                {
                    "id": "platform-static-registry",
                    "path": "contracts/ecosystem/platform-owner.txt",
                    "values": ["pafio-static-registry"],
                },
                {
                    "id": "platform-registry-control-route",
                    "path": "contracts/ecosystem/platform-owner.txt",
                    "values": ["/api/pafio-registry-control/v1"],
                },
                {
                    "id": "platform-worker-command",
                    "path": "contracts/ecosystem/platform-owner.txt",
                    "values": ["pafio build"],
                },
                {
                    "id": "platform-hosted-owner",
                    "path": "contracts/ecosystem/platform-owner.txt",
                    "values": ["hosted-workspace v1"],
                },
            ],
            "forbids": [],
        },
        {
            "name": "vityo-nightly",
            "role": EXPECTED_REPOSITORIES["vityo-nightly"],
            "revision": revisions["vityo-nightly"],
            "required_paths": [
                "products/vityo_app/lib/owner_adapters/pafio_metadata_adapter.dart",
                "products/vityo_app/lib/owner_adapters/styio_compiler_adapter.dart",
                "products/vityo_app/lib/owner_adapters/platform_hosted_adapter.dart",
            ],
            "contains": [
                {
                    "id": "vityo-metadata-adapter",
                    "path":
                        "products/vityo_app/lib/owner_adapters/"
                        "pafio_metadata_adapter.dart",
                    "values": ["pafio metadata --json"],
                },
                {
                    "id": "vityo-compiler-adapter",
                    "path":
                        "products/vityo_app/lib/owner_adapters/"
                        "styio_compiler_adapter.dart",
                    "values": ["styio --machine-info=json"],
                },
                {
                    "id": "vityo-hosted-adapter",
                    "path":
                        "products/vityo_app/lib/owner_adapters/"
                        "platform_hosted_adapter.dart",
                    "values": ["Platform hosted-workspace v1"],
                },
            ],
            "forbids": [
                {
                    "id": "vityo-no-private-home",
                    "path": ".",
                    "values": ["PAFIO_HOME"],
                }
            ],
        },
        {
            "name": "styio.io",
            "role": EXPECTED_REPOSITORIES["styio.io"],
            "revision": revisions["styio.io"],
            "required_paths": ["README.md"],
            "contains": [
                {
                    "id": "site-pafio-entry",
                    "path": "README.md",
                    "values": ["Pafio", "Styio is system-provided"],
                }
            ],
            "forbids": [],
        },
        {
            "name": "styio-audit",
            "role": EXPECTED_REPOSITORIES["styio-audit"],
            "revision": revisions["styio-audit"],
            "required_paths": ["README.md"],
            "contains": [
                {
                    "id": "audit-pafio-scope",
                    "path": "README.md",
                    "values": [
                        "Pafio metadata/workflow",
                        "Platform hosted/registry ownership",
                    ],
                }
            ],
            "forbids": [],
        },
        {
            "name": "styio",
            "role": EXPECTED_REPOSITORIES["styio"],
            "revision": revisions["styio"],
            "required_paths": ["README.md"],
            "contains": [
                {
                    "id": "umbrella-pafio-entry",
                    "path": "README.md",
                    "values": [
                        "Pafio for project workflows",
                        "system Styio for compilation",
                    ],
                }
            ],
            "forbids": [],
        },
    ]


def owner_matrix(revisions: dict[str, str]) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "contracts": contract_map(),
        "repositories": repository_entries(revisions),
    }


def write_matrix(path: Path, matrix: dict[str, Any]) -> str:
    encoded = (
        json.dumps(matrix, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    ).encode("utf-8")
    path.write_bytes(encoded)
    return hashlib.sha256(encoded).hexdigest()


def invoke_verifier(matrix_path: Path, repositories_root: Path) -> subprocess.CompletedProcess[str]:
    return run(
        [
            sys.executable,
            str(VERIFIER),
            "--focused",
            "--matrix",
            str(matrix_path),
            "--repositories-root",
            str(repositories_root),
        ],
        cwd=ROOT,
        env={"ECOSYSTEM_ACCEPTANCE_CHILD": "1"},
    )


def check_success(
    result: subprocess.CompletedProcess[str],
    *,
    expected_digest: str,
    revisions: dict[str, str],
    private_root: Path,
    label: str,
) -> None:
    require(
        result.returncode == 0,
        f"{label}: verifier failed:\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}",
    )
    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        raise AcceptanceFailure(
            f"{label}: verifier success output must be one JSON object: {exc}"
        ) from exc
    require(payload.get("ok") is True, f"{label}: success payload must set ok=true")
    require(payload.get("mode") == "focused", f"{label}: mode must be focused")
    require(
        payload.get("schema_version") == 1,
        f"{label}: schema_version must equal 1",
    )
    require(
        payload.get("contracts") == len(EXPECTED_CONTRACTS),
        f"{label}: contract count drifted",
    )
    require(
        payload.get("repositories") == len(EXPECTED_REPOSITORIES),
        f"{label}: repository count drifted",
    )
    require(
        payload.get("checks") == len(EXPECTED_CHECK_IDS),
        f"{label}: semantic check count drifted",
    )
    require(
        payload.get("matrix_sha256") == expected_digest,
        f"{label}: matrix fingerprint mismatch",
    )
    require(
        payload.get("revisions") == revisions,
        f"{label}: validated revisions are incomplete or reordered semantically",
    )
    private_text = str(private_root)
    require(
        private_text not in result.stdout and private_text not in result.stderr,
        f"{label}: verifier output leaked the repository-root test seam",
    )


def check_failure(
    result: subprocess.CompletedProcess[str],
    *,
    diagnostic: str,
    label: str,
) -> None:
    require(result.returncode != 0, f"{label}: invalid matrix unexpectedly passed")
    combined = f"{result.stdout}\n{result.stderr}".lower()
    require(
        diagnostic.lower() in combined,
        f"{label}: failure did not identify {diagnostic!r}:\n{combined}",
    )


def find_repository(matrix: dict[str, Any], name: str) -> dict[str, Any]:
    for repository in matrix["repositories"]:
        if repository["name"] == name:
            return repository
    raise AcceptanceFailure(f"fixture matrix has no repository {name}")


def run_matrix_case(
    temp_root: Path,
    repositories_root: Path,
    matrix: dict[str, Any],
    *,
    label: str,
    expect_diagnostic: str,
) -> None:
    path = temp_root / f"{label}.json"
    write_matrix(path, matrix)
    check_failure(
        invoke_verifier(path, repositories_root),
        diagnostic=expect_diagnostic,
        label=label,
    )


def main() -> int:
    require(VERIFIER.is_file(), "ecosystem verifier is missing")
    with tempfile.TemporaryDirectory(prefix="pafio-ecosystem-acceptance-") as raw_temp:
        temp_root = Path(raw_temp)
        repositories_root = temp_root / "repositories"
        repositories_root.mkdir()

        revisions: dict[str, str] = {}
        repositories: dict[str, Path] = {}
        for name, files in fixture_sources().items():
            repo, revision = initialize_repository(repositories_root, name, files)
            repositories[name] = repo
            revisions[name] = revision

        # Leave owner worktrees on incompatible commits, then dirty them. A valid
        # result can therefore only come from the revisions pinned in the matrix.
        bad_styio = commit_files(
            repositories["styio-nightly"],
            {
                "tests/interop/pafio-compile-plan.txt":
                    "\n".join(
                        [
                            "compile-plan accepted",
                            "generated_by.tool=pafio",
                            "machine-info v1",
                            "diagnostics v1",
                            "receipt v1",
                            "runtime-events v1",
                            "build_mode=minimal",
                            "",
                        ]
                    )
            },
            "invalid legacy compile-plan profile",
        )
        bad_platform = commit_files(
            repositories["styio-cloud-nightly"],
            {
                "contracts/ecosystem/platform-owner.txt":
                    "\n".join(
                        [
                            "legacy-static-registry",
                            "/api/pafio-registry-control/v1",
                            "worker argv: pafio build",
                            "hosted-workspace v1",
                            "",
                        ]
                    )
            },
            "invalid platform ownership",
        )
        bad_vityo = commit_files(
            repositories["vityo-nightly"],
            {
                "products/vityo_app/lib/owner_adapters/styio_compiler_adapter.dart":
                    "exec: styio --machine-info=json\nread PAFIO_HOME for compiler state\n"
            },
            "invalid private home adapter",
        )
        (
            repositories["styio-nightly"]
            / "tests/interop/pafio-compile-plan.txt"
        ).write_text("dirty worktree must not be observed\n", encoding="utf-8")
        (
            repositories["vityo-nightly"]
            / "products/vityo_app/lib/owner_adapters/pafio_metadata_adapter.dart"
        ).write_text("dirty worktree must not be observed\n", encoding="utf-8")

        accepted = owner_matrix(revisions)
        accepted_path = temp_root / "accepted-owner-matrix.json"
        accepted_digest = write_matrix(accepted_path, accepted)
        check_success(
            invoke_verifier(accepted_path, repositories_root),
            expected_digest=accepted_digest,
            revisions=revisions,
            private_root=temp_root,
            label="fixed-revision replay",
        )

        missing_contract = copy.deepcopy(accepted)
        del missing_contract["contracts"]["receipt"]
        run_matrix_case(
            temp_root,
            repositories_root,
            missing_contract,
            label="missing-contract",
            expect_diagnostic="receipt",
        )

        missing_repository = copy.deepcopy(accepted)
        missing_repository["repositories"] = [
            repository
            for repository in missing_repository["repositories"]
            if repository["name"] != "styio-audit"
        ]
        run_matrix_case(
            temp_root,
            repositories_root,
            missing_repository,
            label="missing-repository",
            expect_diagnostic="styio-audit",
        )

        floating_revision = copy.deepcopy(accepted)
        find_repository(floating_revision, "pafio-nightly")["revision"] = "HEAD"
        run_matrix_case(
            temp_root,
            repositories_root,
            floating_revision,
            label="floating-revision",
            expect_diagnostic="revision",
        )

        missing_check = copy.deepcopy(accepted)
        styio_entry = find_repository(missing_check, "styio-nightly")
        styio_entry["forbids"] = []
        run_matrix_case(
            temp_root,
            repositories_root,
            missing_check,
            label="missing-semantic-check",
            expect_diagnostic="styio-no-build-mode",
        )

        unsafe_path = copy.deepcopy(accepted)
        find_repository(unsafe_path, "styio.io")["required_paths"] = [
            "../outside-owner-repository"
        ]
        run_matrix_case(
            temp_root,
            repositories_root,
            unsafe_path,
            label="unsafe-relative-path",
            expect_diagnostic="path",
        )

        invalid_styio = copy.deepcopy(accepted)
        find_repository(invalid_styio, "styio-nightly")["revision"] = bad_styio
        run_matrix_case(
            temp_root,
            repositories_root,
            invalid_styio,
            label="styio-build-mode",
            expect_diagnostic="styio-no-build-mode",
        )

        invalid_platform = copy.deepcopy(accepted)
        find_repository(
            invalid_platform, "styio-cloud-nightly"
        )["revision"] = bad_platform
        run_matrix_case(
            temp_root,
            repositories_root,
            invalid_platform,
            label="platform-owner-drift",
            expect_diagnostic="platform-static-registry",
        )

        invalid_vityo = copy.deepcopy(accepted)
        find_repository(invalid_vityo, "vityo-nightly")["revision"] = bad_vityo
        run_matrix_case(
            temp_root,
            repositories_root,
            invalid_vityo,
            label="vityo-private-home",
            expect_diagnostic="vityo-no-private-home",
        )

    print(
        json.dumps(
            {
                "ok": True,
                "cases": 9,
                "contracts": len(EXPECTED_CONTRACTS),
                "repositories": len(EXPECTED_REPOSITORIES),
                "checks": len(EXPECTED_CHECK_IDS),
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AcceptanceFailure as exc:
        print(f"ecosystem-owner-contract: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
