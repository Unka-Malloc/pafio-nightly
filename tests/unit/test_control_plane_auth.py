from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import threading
import unittest
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPTS = ROOT / "scripts"
SRC = ROOT / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

import importlib.util

spec = importlib.util.spec_from_file_location(
    "registry_v2_control_plane_server",
    SCRIPTS / "registry-v2-control-plane-server.py",
)
assert spec is not None and spec.loader is not None
control_plane = importlib.util.module_from_spec(spec)
spec.loader.exec_module(control_plane)


class ControlPlaneAuthConfinementTests(unittest.TestCase):
    def _start_server(self, *, auth_token: str, staging_dir: pathlib.Path, root: pathlib.Path, key_dir: pathlib.Path):
        handler = control_plane.RegistryControlPlaneHandler
        handler.registry_root = str(root)
        handler.key_dir = str(key_dir)
        handler.registry_name = "unit-control"
        handler.spio_bin = str(ROOT / "scripts" / "spio")
        handler.read_root_url = ""
        handler.control_plane_base_url = ""
        handler.auth_token = auth_token
        handler.staging_dir = str(staging_dir.resolve())
        server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        return server

    def _request(self, server: ThreadingHTTPServer, method: str, path: str, body: dict | None = None, token: str | None = None):
        conn = HTTPConnection("127.0.0.1", server.server_address[1], timeout=5)
        headers = {"Content-Type": "application/json"}
        if token is not None:
            headers["Authorization"] = f"Bearer {token}"
        payload = b""
        if body is not None:
            payload = json.dumps(body).encode("utf-8")
            headers["Content-Length"] = str(len(payload))
        conn.request(method, path, body=payload, headers=headers)
        response = conn.getresponse()
        raw = response.read().decode("utf-8")
        conn.close()
        return response.status, json.loads(raw)

    def test_unauthenticated_and_wrong_token_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            staging = root / "staging"
            staging.mkdir()
            server = self._start_server(
                auth_token="correct-token",
                staging_dir=staging,
                root=root / "registry",
                key_dir=root / "keys",
            )
            try:
                status, payload = self._request(
                    server,
                    "POST",
                    "/api/spio-registry-control/v1/publish",
                    body={"manifest_path": "pkg/spio.toml"},
                )
                self.assertEqual(status, 401)
                self.assertEqual(payload["error_payload"]["category"], "AuthError")

                status, payload = self._request(
                    server,
                    "POST",
                    "/api/spio-registry-control/v1/publish",
                    body={"manifest_path": "pkg/spio.toml"},
                    token="wrong-token",
                )
                self.assertEqual(status, 401)
                self.assertEqual(payload["error_payload"]["category"], "AuthError")
            finally:
                server.shutdown()
                server.server_close()

    def test_deny_by_default_when_token_unconfigured(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            staging = root / "staging"
            staging.mkdir()
            server = self._start_server(
                auth_token="",
                staging_dir=staging,
                root=root / "registry",
                key_dir=root / "keys",
            )
            try:
                status, payload = self._request(
                    server,
                    "POST",
                    "/api/spio-registry-control/v1/publish",
                    body={"manifest_path": "pkg/spio.toml"},
                    token="anything",
                )
                self.assertEqual(status, 401)
                self.assertIn("deny-by-default", payload["error_payload"]["detail"])
            finally:
                server.shutdown()
                server.server_close()

    def test_path_confinement_rejects_absolute_and_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            staging = root / "staging"
            staging.mkdir()
            outside = root / "outside" / "spio.toml"
            outside.parent.mkdir()
            outside.write_text("x", encoding="utf-8")
            server = self._start_server(
                auth_token="correct-token",
                staging_dir=staging,
                root=root / "registry",
                key_dir=root / "keys",
            )
            try:
                status, payload = self._request(
                    server,
                    "POST",
                    "/api/spio-registry-control/v1/publish",
                    body={"manifest_path": str(outside)},
                    token="correct-token",
                )
                self.assertEqual(status, 400)
                self.assertEqual(payload["error_payload"]["category"], "PathConfinementError")

                status, payload = self._request(
                    server,
                    "POST",
                    "/api/spio-registry-control/v1/publish",
                    body={"manifest_path": "../outside/spio.toml"},
                    token="correct-token",
                )
                self.assertEqual(status, 400)
                self.assertEqual(payload["error_payload"]["category"], "PathConfinementError")
            finally:
                server.shutdown()
                server.server_close()

    def test_structured_failure_status_codes(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = pathlib.Path(temp_dir)
            staging = root / "staging"
            staging.mkdir()
            server = self._start_server(
                auth_token="correct-token",
                staging_dir=staging,
                root=root / "registry",
                key_dir=root / "keys",
            )
            try:
                status, payload = self._request(
                    server,
                    "POST",
                    "/api/spio-registry-control/v1/publish",
                    body="not-json",  # type: ignore[arg-type]
                    token="correct-token",
                )
            except Exception:
                # load_json_request path via raw body: send invalid JSON manually
                conn = HTTPConnection("127.0.0.1", server.server_address[1], timeout=5)
                body = b"not-json"
                conn.request(
                    "POST",
                    "/api/spio-registry-control/v1/publish",
                    body=body,
                    headers={
                        "Content-Type": "application/json",
                        "Content-Length": str(len(body)),
                        "Authorization": "Bearer correct-token",
                    },
                )
                response = conn.getresponse()
                status = response.status
                payload = json.loads(response.read().decode("utf-8"))
                conn.close()
                self.assertEqual(status, 400)
                self.assertEqual(payload["error_payload"]["category"], "UsageError")
                self.assertIn("returncode", payload)
            finally:
                server.shutdown()
                server.server_close()


if __name__ == "__main__":
    unittest.main()
