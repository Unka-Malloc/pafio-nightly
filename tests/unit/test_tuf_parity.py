from __future__ import annotations

import json
import pathlib
import shutil
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))

from spio_registry_v2 import generate_key_directory, publish_to_registry_v2, verify_registry_root  # noqa: E402
from spio_registry_v2.common import RegistryV2Error, sha256_file  # noqa: E402
from spio_registry_v2.validator import RootReader  # noqa: E402

import tarfile


class TufParityAndTamperTests(unittest.TestCase):
    def _build_source_archive(self, package_name: str, version: str, destination: pathlib.Path) -> None:
        short_name = package_name.split("/", 1)[1]
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_root = pathlib.Path(temp_dir)
            package_root = temp_root / f"{short_name}-{version}"
            (package_root / "src").mkdir(parents=True)
            (package_root / "spio.toml").write_text(
                "\n".join(
                    [
                        "[spio]",
                        "manifest-version = 1",
                        "",
                        "[package]",
                        f'name = "{package_name}"',
                        f'version = "{version}"',
                        'edition = "2026"',
                        "publish = true",
                        "",
                        "[build]",
                        "implicit-std = true",
                        "",
                        "[lib]",
                        'path = "src/lib.styio"',
                        "",
                    ]
                ),
                encoding="utf-8",
            )
            (package_root / "src" / "lib.styio").write_text(f"# {package_name}@{version}\n", encoding="utf-8")
            destination.parent.mkdir(parents=True, exist_ok=True)
            with tarfile.open(destination, mode="w") as archive:
                archive.add(package_root, arcname=package_root.name)

    def _publish_fixture(self, root: pathlib.Path) -> pathlib.Path:
        dest_root = root / "registry-v2"
        key_dir = root / "keys"
        archive = root / "artifacts" / "util-1.0.0.tar"
        self._build_source_archive("acme/util", "1.0.0", archive)
        generate_key_directory(key_dir)
        publish_to_registry_v2(
            str(dest_root),
            str(key_dir),
            archive_path_value=str(archive),
            registry_name="tuf-parity",
            publisher_id="unit-test",
        )
        return dest_root

    def test_valid_registry_verifies(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            dest_root = self._publish_fixture(pathlib.Path(temp_dir))
            verified = verify_registry_root(str(dest_root))
            self.assertTrue(verified["ok"])

    def test_tampered_role_files_fail_closed(self) -> None:
        roles = (
            "trust/root.json",
            "trust/timestamp.json",
            "trust/snapshot.json",
            "trust/targets/acme.json",
        )
        for relative in roles:
            with self.subTest(relative=relative):
                with tempfile.TemporaryDirectory() as temp_dir:
                    dest_root = self._publish_fixture(pathlib.Path(temp_dir))
                    target = dest_root / relative
                    payload = json.loads(target.read_text(encoding="utf-8"))
                    if "signed" in payload and isinstance(payload["signed"], dict):
                        payload["signed"]["version"] = int(payload["signed"].get("version", 1)) + 99
                    else:
                        payload["tampered"] = True
                    target.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
                    with self.assertRaises(RegistryV2Error):
                        verify_registry_root(str(dest_root))

    def test_tampered_snapshot_hash_link_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            dest_root = self._publish_fixture(pathlib.Path(temp_dir))
            snapshot_path = dest_root / "trust" / "snapshot.json"
            # Corrupt the targets file bytes without resigning snapshot.
            targets_path = dest_root / "trust" / "targets" / "acme.json"
            targets_path.write_text(targets_path.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            with self.assertRaises(RegistryV2Error):
                verify_registry_root(str(dest_root))
            # Snapshot itself remains parseable; failure is hash mismatch.
            self.assertTrue(snapshot_path.exists())

    def test_zero_root_signature_threshold_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            dest_root = self._publish_fixture(pathlib.Path(temp_dir))
            root_path = dest_root / "trust" / "root.json"
            payload = json.loads(root_path.read_text(encoding="utf-8"))
            payload["signed"]["roles"]["root"]["threshold"] = 0
            root_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

            with self.assertRaisesRegex(RegistryV2Error, "threshold must be >= 1"):
                verify_registry_root(str(dest_root))

    def test_offline_reader_uses_cached_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            dest_root = self._publish_fixture(pathlib.Path(temp_dir))
            reader = RootReader(str(dest_root))
            root_bytes = reader.read_bytes("trust/root.json")
            self.assertGreater(len(root_bytes), 0)
            # Second read hits cache (same object identity path).
            self.assertEqual(reader.read_bytes("trust/root.json"), root_bytes)
            self.assertEqual(sha256_file(dest_root / "trust" / "root.json"), reader.object_sha256("trust/root.json"))

    def test_parity_fixture_layout_stable(self) -> None:
        """Shared fixture shape consumed by native TufVerifier parity tests."""
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            dest_root = self._publish_fixture(root)
            fixture_dir = root / "parity-fixture"
            shutil.copytree(dest_root, fixture_dir)
            required = [
                "config.json",
                "trust/root.json",
                "trust/timestamp.json",
                "trust/snapshot.json",
                "trust/targets/acme.json",
                "log/checkpoint.json",
            ]
            for relative in required:
                self.assertTrue((fixture_dir / relative).is_file(), relative)
            verify_registry_root(str(fixture_dir))


if __name__ == "__main__":
    unittest.main()
