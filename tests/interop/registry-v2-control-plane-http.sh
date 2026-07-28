#!/usr/bin/env bash
set -euo pipefail

SPIO_BIN="${1:?expected spio binary path}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ROOT="$(mktemp -d)"
LOG_FILE="$ROOT/control-plane.log"
SERVER_PID=""
AUTH_TOKEN="interop-control-token"

cleanup() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
  fi
  rm -rf "$ROOT"
}
trap cleanup EXIT

export SPIO_HOME="$ROOT/.spio-home"
KEY_DIR="$ROOT/keys"
V2_ROOT="$ROOT/registry-v2"
STAGING_DIR="$ROOT/staging"
PACKAGE_ROOT="$STAGING_DIR/publish/app"
mkdir -p "$PACKAGE_ROOT/src"

cat >"$PACKAGE_ROOT/spio.toml" <<'EOF'
[spio]
manifest-version = 1

[package]
name = "demo/app"
version = "0.1.0"
edition = "2026"
publish = true

[toolchain]
channel = "nightly"
implicit-std = true

[lib]
path = "src/lib.styio"
EOF

cat >"$PACKAGE_ROOT/src/lib.styio" <<'EOF'
# app
EOF

python3 "$REPO_ROOT/scripts/registry-v2-keygen.py" --output-dir "$KEY_DIR" >/dev/null

PORT="$(
  python3 - <<'PY'
import socket
s = socket.socket()
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PY
)"

python3 "$REPO_ROOT/scripts/registry-v2-control-plane-server.py" \
  --root "$V2_ROOT" \
  --key-dir "$KEY_DIR" \
  --spio-bin "$SPIO_BIN" \
  --registry-name "http-control-registry" \
  --auth-token "$AUTH_TOKEN" \
  --staging-dir "$STAGING_DIR" \
  --bind 127.0.0.1 \
  --port "$PORT" >"$LOG_FILE" 2>&1 &
SERVER_PID="$!"

BASE_URL="http://127.0.0.1:${PORT}/api/spio-registry-control/v1"
AUTH_HEADER="Authorization: Bearer ${AUTH_TOKEN}"
for _ in $(seq 1 30); do
  if curl -fsS "${BASE_URL}/status" >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

STATUS_BEFORE="$(curl -fsS "${BASE_URL}/status")"
python3 - "$STATUS_BEFORE" <<'PY'
import json
import sys
payload = json.loads(sys.argv[1])
assert payload["returncode"] == 0
assert payload["payload"]["root_initialized"] is False
assert payload["payload"]["registry_root"] == "<redacted>"
assert payload["payload"]["key_dir"] == "<redacted>"
assert payload["payload"]["auth_required"] is True
PY

UNAUTH_CODE="$(
  curl -sS -o "$ROOT/unauth-response.json" -w '%{http_code}' -X POST "${BASE_URL}/publish" \
    -H 'Content-Type: application/json' \
    --data "{\"manifest_path\": \"publish/app/spio.toml\", \"publisher_id\": \"http-test\"}"
)"
test "$UNAUTH_CODE" = "401"

WRONG_TOKEN_CODE="$(
  curl -sS -o "$ROOT/wrong-token-response.json" -w '%{http_code}' -X POST "${BASE_URL}/publish" \
    -H 'Content-Type: application/json' \
    -H 'Authorization: Bearer wrong-token' \
    --data "{\"manifest_path\": \"publish/app/spio.toml\", \"publisher_id\": \"http-test\"}"
)"
test "$WRONG_TOKEN_CODE" = "401"

TRAVERSAL_CODE="$(
  curl -sS -o "$ROOT/traversal-response.json" -w '%{http_code}' -X POST "${BASE_URL}/publish" \
    -H 'Content-Type: application/json' \
    -H "$AUTH_HEADER" \
    --data "{\"manifest_path\": \"../keys/keys.json\", \"publisher_id\": \"http-test\"}"
)"
test "$TRAVERSAL_CODE" = "400"
python3 - "$ROOT/traversal-response.json" <<'PY'
import json
import sys
payload = json.loads(open(sys.argv[1], encoding="utf-8").read())
assert payload["error_payload"]["category"] == "PathConfinementError"
PY

PUBLISH_JSON="$(
  curl -fsS -X POST "${BASE_URL}/publish" \
    -H 'Content-Type: application/json' \
    -H "$AUTH_HEADER" \
    --data "{\"manifest_path\": \"publish/app/spio.toml\", \"publisher_id\": \"http-test\"}"
)"

python3 - "$PUBLISH_JSON" <<'PY'
import json
import sys
payload = json.loads(sys.argv[1])
assert payload["returncode"] == 0
assert payload["payload"]["created_root"] is True
assert payload["payload"]["package"] == "demo/app"
assert payload["payload"]["version"] == "0.1.0"
PY

STATUS_AFTER="$(curl -fsS "${BASE_URL}/status")"
python3 - "$STATUS_AFTER" <<'PY'
import json
import sys
payload = json.loads(sys.argv[1])
assert payload["returncode"] == 0
assert payload["payload"]["root_initialized"] is True
assert payload["payload"]["config_present"] is True
assert payload["payload"]["root_metadata_present"] is True
assert payload["payload"]["registry_root"] == "<redacted>"
assert payload["payload"]["key_dir"] == "<redacted>"
PY

OVERSIZED_CODE="$(
  python3 - <<'PY' | curl -sS -o "$ROOT/oversized-response.json" -w '%{http_code}' -X POST "${BASE_URL}/publish" -H 'Content-Type: application/json' -H "Authorization: Bearer interop-control-token" --data-binary @-
print("x" * 1048577)
PY
)"

test "$OVERSIZED_CODE" = "400"

python3 - "$ROOT/oversized-response.json" <<'PY'
import json
import sys
payload = json.loads(open(sys.argv[1], encoding="utf-8").read())
assert payload["returncode"] == 2
assert payload["error_payload"]["category"] == "UsageError"
PY

VERIFY_JSON="$(
  curl -fsS -X POST "${BASE_URL}/verify" \
    -H 'Content-Type: application/json' \
    -H "$AUTH_HEADER" \
    --data '{}'
)"

python3 - "$VERIFY_JSON" <<'PY'
import json
import sys
payload = json.loads(sys.argv[1])
assert payload["returncode"] == 0
assert payload["payload"]["ok"] is True
assert payload["payload"]["releases"] == 1
PY

VERIFY_BAD_CODE="$(
  curl -sS -o "$ROOT/verify-bad-response.json" -w '%{http_code}' -X POST "${BASE_URL}/verify" \
    -H 'Content-Type: application/json' \
    -H "$AUTH_HEADER" \
    --data '{"unexpected": true}'
)"

test "$VERIFY_BAD_CODE" = "400"

python3 - "$ROOT/verify-bad-response.json" <<'PY'
import json
import sys
payload = json.loads(open(sys.argv[1], encoding="utf-8").read())
assert payload["returncode"] == 2
assert payload["error_payload"]["category"] == "UsageError"
PY

DUPLICATE_CODE="$(
  curl -sS -o "$ROOT/duplicate-response.json" -w '%{http_code}' -X POST "${BASE_URL}/publish" \
    -H 'Content-Type: application/json' \
    -H "$AUTH_HEADER" \
    --data "{\"manifest_path\": \"publish/app/spio.toml\", \"publisher_id\": \"http-test\"}"
)"

test "$DUPLICATE_CODE" = "409"

python3 - "$ROOT/duplicate-response.json" <<'PY'
import json
import sys
payload = json.loads(open(sys.argv[1], encoding="utf-8").read())
assert payload["returncode"] != 0
assert payload["error_payload"]["category"] == "PublishError"
PY
