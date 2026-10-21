#!/usr/bin/env python3
"""Retain this milestone's host command and dirty inputs; no check selection/retry logic."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = ROOT / "build-codex/task-resource-final"
ENV_KEYS = (
    "CI", "PATH", "CC", "CXX", "SDKROOT", "MACOSX_DEPLOYMENT_TARGET",
    "CMAKE_GENERATOR", "CMAKE_BUILD_PARALLEL_LEVEL", "CTEST_PARALLEL_LEVEL",
    "CMAKE_PREFIX_PATH", "CMAKE_TOOLCHAIN_FILE", "CTEST_OUTPUT_ON_FAILURE",
    "PAFIO_BUILD_DIR", "PAFIO_STYIO_BIN", "STYIO_BIN", "STYIO_LSPD_BIN",
    "FETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS", "FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON",
    "FETCHCONTENT_SOURCE_DIR_GOOGLETEST",
)


def selected_inputs() -> dict:
    selected = {}
    for name, path in (
        ("pafio", ROOT / "build-codex/bin/pafio"),
        ("styio", ROOT / "../styio-nightly/build/default/bin/styio"),
        ("styio_lspd", ROOT / "../styio-nightly/build/default/bin/styio_lspd"),
    ):
        path = path.resolve()
        info = path.stat()
        selected[name] = {"path": str(path), "size": info.st_size, "mtime_ns": info.st_mtime_ns}
    return selected


def retain_inputs(directory: Path) -> None:
    directory.mkdir()
    for name, args in (
        ("revision.txt", ["rev-parse", "HEAD"]),
        ("branch.txt", ["branch", "--show-current"]),
        ("status.txt", ["status", "--short", "--untracked-files=all"]),
        ("tracked.patch", ["diff", "HEAD", "--binary"]),
    ):
        with (directory / name).open("wb") as output:
            subprocess.run(["git", *args], cwd=ROOT, stdout=output, check=True)
    paths = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=ROOT)
    for raw in paths.split(b"\0"):
        if raw:
            relative = Path(os.fsdecode(raw))
            target = directory / "untracked" / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target, follow_symlinks=False)
    for name in ("CMakeCache.txt", "compile_commands.json"):
        path = ROOT / "build-codex" / name
        if path.exists():
            shutil.copy2(path, directory / name)
    (directory / "selected.json").write_text(json.dumps(selected_inputs(), indent=2) + "\n")
    (directory / "environment.json").write_text(json.dumps({
        key: os.environ[key] for key in ENV_KEYS if key in os.environ
    }, indent=2) + "\n")


def main() -> int:
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    prepare = sys.argv[1:] == ["--prepare-only"]
    directory = Path(tempfile.mkdtemp(prefix="prepared-" if prepare else "host-", dir=EVIDENCE))
    (EVIDENCE / ("prepared.txt" if prepare else "latest.txt")).write_text(str(directory.relative_to(ROOT)) + "\n")
    retain_inputs(directory / "inputs")
    if prepare:
        print(f"Prepared input record: {directory.relative_to(ROOT)}")
        return 0
    command = sys.argv[1:]
    if not command:
        raise ValueError("an explicit host command is required")
    env = dict(os.environ, PAFIO_GATE_LOG_DIR=str(directory),
               PAFIO_BUILD_DIR=str(ROOT / "build-codex"),
               PAFIO_STYIO_BIN=str(ROOT / "../styio-nightly/build/default/bin/styio"))
    (directory / "command.json").write_text(json.dumps({
        "command": command, "cwd": str(ROOT),
        "environment": {key: env[key] for key in (*ENV_KEYS, "PAFIO_GATE_LOG_DIR") if key in env},
    }, indent=2) + "\n")
    with (directory / "stdout.log").open("wb") as stdout, (directory / "stderr.log").open("wb") as stderr:
        proc = subprocess.run(command, cwd=ROOT, env=env, stdout=stdout, stderr=stderr)
    (directory / "process.exit").write_text(f"{proc.returncode}\n")
    retention_rc = 0
    try:
        (directory / "selected-after.json").write_text(json.dumps(selected_inputs(), indent=2) + "\n")
    except OSError as error:
        (directory / "retention-error.log").write_text(str(error) + "\n")
        retention_rc = 1
    (directory / "retention.exit").write_text(f"{retention_rc}\n")
    rc = proc.returncode if proc.returncode >= 0 else 128 - proc.returncode
    if rc == 0:
        rc = retention_rc
    (directory / "check.exit").write_text(f"{rc}\n")
    # Do not print private child output to the bounded shared host console.
    print(f"Retained command evidence: {directory.relative_to(ROOT)}; process exit {proc.returncode}")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
