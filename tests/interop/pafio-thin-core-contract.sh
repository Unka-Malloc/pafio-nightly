#!/usr/bin/env bash
set -euo pipefail

SPIO_BIN="${1:?expected spio binary path}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ROOT="$(mktemp -d)"
SERVER_PID=""
LOG_FILE="$ROOT/http-server.log"

cleanup() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
  fi
  rm -rf "$ROOT"
}
trap cleanup EXIT

fail() {
  printf 'pafio-thin-core-contract: %s\n' "$*" >&2
  exit 1
}

export SPIO_HOME="$ROOT/.spio-home"
REGISTRY_ROOT="$ROOT/registry-v2"
KEY_DIR="$ROOT/keys"
PUBLISH_ROOT="$ROOT/publish/util"
mkdir -p "$PUBLISH_ROOT/src"

cat >"$PUBLISH_ROOT/spio.toml" <<'EOF'
[spio]
manifest-version = 1

[package]
name = "acme/util"
version = "0.2.0"
edition = "2026"
publish = true

[build]
implicit-std = true

[lib]
path = "src/lib.styio"
EOF
printf '# util\n' >"$PUBLISH_ROOT/src/lib.styio"

python3 "$REPO_ROOT/scripts/registry-v2-keygen.py" --output-dir "$KEY_DIR" >/dev/null
python3 "$REPO_ROOT/scripts/registry-v2-publish.py" \
  --root "$REGISTRY_ROOT" \
  --key-dir "$KEY_DIR" \
  --manifest-path "$PUBLISH_ROOT/spio.toml" \
  --spio-bin "$SPIO_BIN" \
  --registry-name "thin-core-fixture" >/dev/null

ARTIFACT_INFO="$(
  python3 - "$REGISTRY_ROOT/index/acme/util.jsonl" <<'PY'
import json
import pathlib
import sys

record = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").strip())
print(record["source_artifact"]["path"] + "\t" + record["source_artifact"]["sha256"])
PY
)"
IFS=$'\t' read -r ARTIFACT_PATH ARTIFACT_SHA256 <<<"$ARTIFACT_INFO"

PORT="$(
  python3 - <<'PY'
import socket

sock = socket.socket()
sock.bind(("127.0.0.1", 0))
print(sock.getsockname()[1])
sock.close()
PY
)"
REGISTRY_URL="http://127.0.0.1:${PORT}"

start_server() {
  python3 -m http.server "$PORT" \
    --bind 127.0.0.1 \
    --directory "$REGISTRY_ROOT" >>"$LOG_FILE" 2>&1 &
  SERVER_PID="$!"
  for _ in $(seq 1 50); do
    if curl -fsS "${REGISTRY_URL}/config.json" >/dev/null 2>&1; then
      return
    fi
    sleep 0.1
  done
  fail "localhost registry did not become ready"
}

stop_server() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
    SERVER_PID=""
  fi
}

start_server

python3 - \
  "$REGISTRY_URL" \
  "$REGISTRY_ROOT/trust/root.json" \
  "$ROOT/registry-descriptor.json" <<'PY'
import hashlib
import json
import pathlib
import sys
from datetime import datetime, timedelta, timezone

registry_url = sys.argv[1]
root_bytes = pathlib.Path(sys.argv[2]).read_bytes()
descriptor_path = pathlib.Path(sys.argv[3])
issued_at = datetime.now(timezone.utc)
descriptor_path.write_text(
    json.dumps(
        {
            "schema_version": 1,
            "registry_name": "thin-core-fixture",
            "registry_root": registry_url,
            "control_plane_base_url": "localhost-test-fixture",
            "root_sha256": hashlib.sha256(root_bytes).hexdigest(),
            "issued_at": issued_at.strftime("%Y-%m-%dT%H:%M:%SZ"),
            "expires": (issued_at + timedelta(days=1)).strftime("%Y-%m-%dT%H:%M:%SZ"),
        },
        sort_keys=True,
    )
    + "\n",
    encoding="utf-8",
)
PY
"$SPIO_BIN" --json registry trust import --dev "$ROOT/registry-descriptor.json" >/dev/null

write_project() {
  local project_root="$1"
  local app_name="$2"
  mkdir -p "$project_root/src" "$project_root/vendor/bridge/src"
  cat >"$project_root/spio.toml" <<EOF
[spio]
manifest-version = 1

[package]
name = "acme/${app_name}"
version = "0.1.0"
edition = "2026"
publish = false

[build]
implicit-std = true

[[bin]]
name = "${app_name}"
path = "src/main.styio"

[dependencies]
bridge = { package = "acme/bridge", path = "vendor/bridge" }
EOF
  cat >"$project_root/vendor/bridge/spio.toml" <<EOF
[spio]
manifest-version = 1

[package]
name = "acme/bridge"
version = "0.1.0"
edition = "2026"
publish = false

[build]
implicit-std = true

[lib]
path = "src/lib.styio"

[dependencies]
util = { package = "acme/util", version = "0.2.0", registry = "${REGISTRY_URL}" }
EOF
  printf '# app\n' >"$project_root/src/main.styio"
  printf '# bridge\n' >"$project_root/vendor/bridge/src/lib.styio"
}

PROJECT_A="$ROOT/project-a"
PROJECT_B="$ROOT/project-b"
write_project "$PROJECT_A" "app-a"
write_project "$PROJECT_B" "app-b"

# Two projects race for one digest. The localhost log is the acquisition
# oracle; merely finding one final file would not detect duplicate downloads.
: >"$LOG_FILE"
"$SPIO_BIN" sync --manifest-path "$PROJECT_A/spio.toml" \
  >"$ROOT/sync-a.stdout" 2>"$ROOT/sync-a.stderr" &
SYNC_A_PID="$!"
"$SPIO_BIN" sync --manifest-path "$PROJECT_B/spio.toml" \
  >"$ROOT/sync-b.stdout" 2>"$ROOT/sync-b.stderr" &
SYNC_B_PID="$!"

set +e
wait "$SYNC_A_PID"
SYNC_A_STATUS="$?"
wait "$SYNC_B_PID"
SYNC_B_STATUS="$?"
set -e
if [[ "$SYNC_A_STATUS" -ne 0 || "$SYNC_B_STATUS" -ne 0 ]]; then
  cat "$ROOT/sync-a.stderr" "$ROOT/sync-b.stderr" >&2
  fail "concurrent sync failed"
fi

ARTIFACT_GET_COUNT="$(
  python3 - "$LOG_FILE" "$ARTIFACT_PATH" <<'PY'
import pathlib
import sys

needle = f"GET /{sys.argv[2]} "
print(sum(1 for line in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").splitlines() if needle in line))
PY
)"
[[ "$ARTIFACT_GET_COUNT" == "1" ]] ||
  fail "expected one artifact GET for shared digest, got $ARTIFACT_GET_COUNT"

BLOB_PATH="$SPIO_HOME/registry/blobs/sha256/${ARTIFACT_SHA256:0:2}/${ARTIFACT_SHA256:2:2}/${ARTIFACT_SHA256}.tar"
CHECKOUT_ROOT="$SPIO_HOME/registry/checkouts/acme/util/0.2.0/$ARTIFACT_SHA256"
READY_MARKER="$CHECKOUT_ROOT/.spio-snapshot-ready"
[[ -f "$BLOB_PATH" ]] || fail "verified CAS blob is missing"
[[ -f "$READY_MARKER" ]] || fail "ready checkout marker is missing"
python3 - "$BLOB_PATH" "$ARTIFACT_SHA256" <<'PY'
import hashlib
import pathlib
import sys

actual = hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest()
if actual != sys.argv[2]:
    raise SystemExit(f"blob digest mismatch: {actual}")
PY
if find "$SPIO_HOME/registry" -name '*.tmp.*' -print -quit | grep -q .; then
  fail "registry cache contains an abandoned temporary object"
fi

for project in "$PROJECT_A" "$PROJECT_B"; do
  [[ -f "$project/spio.lock" ]] || fail "sync did not commit lockfile"
  [[ -f "$project/.spio/resolution-v1.json" ]] ||
    fail "sync did not commit resolution-v1"
done

cat >"$ROOT/styio-resolution-consumer.py" <<'PY'
import hashlib
import json
import pathlib
import re
import sys

SHA256 = re.compile(r"^[0-9a-f]{64}$")
TOP_KEYS = {"schema_version", "manifest_sha256", "lock_sha256", "roots", "packages"}
PACKAGE_KEYS = {"id", "root", "content_sha256", "dependencies"}
DEPENDENCY_KEYS = {"alias", "package_id"}


def reject(code):
    print(f"resolution-v1:{code}", file=sys.stderr)
    raise SystemExit(1)


resolution_path, manifest_path, lock_path = map(pathlib.Path, sys.argv[1:4])
try:
    document = json.loads(resolution_path.read_text(encoding="utf-8"))
except Exception:
    reject("json")

if set(document) != TOP_KEYS:
    reject("top_fields")
if document["schema_version"] != 1:
    reject("schema_version")
if document["manifest_sha256"] != hashlib.sha256(manifest_path.read_bytes()).hexdigest():
    reject("manifest_stale")
if document["lock_sha256"] != hashlib.sha256(lock_path.read_bytes()).hexdigest():
    reject("lock_stale")
if document["roots"] != sorted(document["roots"]) or len(document["roots"]) != len(set(document["roots"])):
    reject("roots")
if not isinstance(document["packages"], list):
    reject("packages")

package_ids = [package.get("id") for package in document["packages"]]
if package_ids != sorted(package_ids) or len(package_ids) != len(set(package_ids)):
    reject("package_ids")
known_ids = set(package_ids)
if not set(document["roots"]).issubset(known_ids):
    reject("root_reference")

aliases_by_id = {}
for package in document["packages"]:
    if set(package) != PACKAGE_KEYS:
        reject("package_fields")
    root = pathlib.Path(package["root"])
    if not root.is_absolute() or not root.is_dir():
        reject("package_root")
    content_sha256 = package["content_sha256"]
    if package["id"].startswith("registry:"):
        if not isinstance(content_sha256, str) or not SHA256.fullmatch(content_sha256):
            reject("registry_digest")
        if not package["id"].endswith("#" + content_sha256):
            reject("registry_digest_identity")
    elif content_sha256 is not None:
        reject("mutable_digest")
    dependencies = package["dependencies"]
    aliases = [dependency.get("alias") for dependency in dependencies]
    if aliases != sorted(aliases) or len(aliases) != len(set(aliases)):
        reject("aliases")
    mapping = {}
    for dependency in dependencies:
        if set(dependency) != DEPENDENCY_KEYS:
            reject("dependency_fields")
        if dependency["package_id"] not in known_ids:
            reject("dependency_reference")
        mapping[dependency["alias"]] = dependency["package_id"]
    aliases_by_id[package["id"]] = mapping

for root_id in document["roots"]:
    bridge_id = aliases_by_id[root_id].get("bridge")
    if bridge_id is None:
        reject("consumer_bridge")
    util_id = aliases_by_id[bridge_id].get("util")
    if util_id is None or not util_id.startswith("registry:acme/util@0.2.0#"):
        reject("consumer_util")
PY

python3 \
  "$ROOT/styio-resolution-consumer.py" \
  "$PROJECT_A/.spio/resolution-v1.json" \
  "$PROJECT_A/spio.toml" \
  "$PROJECT_A/spio.lock"

cp "$PROJECT_A/spio.lock" "$ROOT/expected.lock"
cp "$PROJECT_A/.spio/resolution-v1.json" "$ROOT/expected-resolution.json"
READY_STAT_BEFORE="$(
  python3 - "$READY_MARKER" <<'PY'
import pathlib
import sys

stat = pathlib.Path(sys.argv[1]).stat()
print(f"{stat.st_ino}:{stat.st_size}:{stat.st_mtime_ns}")
PY
)"

# A stopped localhost server makes the no-network oracle independent of logs or
# implementation counters. The marker stat catches a hidden re-extraction.
stop_server
"$SPIO_BIN" sync --locked --offline --manifest-path "$PROJECT_A/spio.toml" >/dev/null
cmp -s "$PROJECT_A/spio.lock" "$ROOT/expected.lock" ||
  fail "warm offline sync rewrote lockfile"
cmp -s "$PROJECT_A/.spio/resolution-v1.json" "$ROOT/expected-resolution.json" ||
  fail "warm offline sync rewrote resolution"
READY_STAT_AFTER="$(
  python3 - "$READY_MARKER" <<'PY'
import pathlib
import sys

stat = pathlib.Path(sys.argv[1]).stat()
print(f"{stat.st_ino}:{stat.st_size}:{stat.st_mtime_ns}")
PY
)"
[[ "$READY_STAT_AFTER" == "$READY_STAT_BEFORE" ]] ||
  fail "warm offline sync re-extracted the registry package"

# Corruption is an explicit offline failure, never an accepted cache hit.
printf 'corrupt\n' >"$BLOB_PATH"
if "$SPIO_BIN" sync --locked --offline --manifest-path "$PROJECT_A/spio.toml" \
  >"$ROOT/corrupt-offline.stdout" 2>"$ROOT/corrupt-offline.stderr"; then
  fail "offline sync accepted a corrupt cached artifact"
fi
grep -Eq 'sha256 mismatch|corrupt|integrity' "$ROOT/corrupt-offline.stderr" ||
  fail "offline corruption did not produce a stable integrity diagnostic"
cmp -s "$PROJECT_A/spio.lock" "$ROOT/expected.lock" ||
  fail "offline corruption changed lockfile"
cmp -s "$PROJECT_A/.spio/resolution-v1.json" "$ROOT/expected-resolution.json" ||
  fail "offline corruption changed resolution"

# Online mode must discard and reacquire the one corrupt digest.
: >"$LOG_FILE"
start_server
"$SPIO_BIN" sync --locked --manifest-path "$PROJECT_A/spio.toml" >/dev/null
python3 - "$BLOB_PATH" "$ARTIFACT_SHA256" <<'PY'
import hashlib
import pathlib
import sys

actual = hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest()
if actual != sys.argv[2]:
    raise SystemExit(f"online repair did not restore digest: {actual}")
PY
cmp -s "$PROJECT_A/spio.lock" "$ROOT/expected.lock" ||
  fail "online cache repair changed lockfile"
cmp -s "$PROJECT_A/.spio/resolution-v1.json" "$ROOT/expected-resolution.json" ||
  fail "online cache repair changed canonical resolution"

# Produce single-defect resolution copies for the minimal Styio consumer.
python3 - "$PROJECT_A/.spio/resolution-v1.json" "$ROOT" <<'PY'
import copy
import json
import pathlib
import sys

source = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
root = pathlib.Path(sys.argv[2])


def emit(name, mutate):
    value = copy.deepcopy(source)
    mutate(value)
    (root / f"bad-{name}.json").write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


emit("schema", lambda value: value.__setitem__("schema_version", 2))
emit("manifest", lambda value: value.__setitem__("manifest_sha256", "0" * 64))
emit("lock", lambda value: value.__setitem__("lock_sha256", "0" * 64))
emit("duplicate", lambda value: value["packages"].append(copy.deepcopy(value["packages"][0])))
emit(
    "dangling",
    lambda value: value["packages"][0]["dependencies"][0].__setitem__(
        "package_id",
        "registry:acme/missing@1.0.0#" + "d" * 64,
    ),
)
emit("root", lambda value: value["packages"][0].__setitem__("root", str(root / "missing-root")))
emit("boundary", lambda value: value.__setitem__("toolchain", {"channel": "nightly"}))
PY

expect_reject() {
  local file="$1"
  local code="$2"
  if python3 "$ROOT/styio-resolution-consumer.py" \
    "$file" "$PROJECT_A/spio.toml" "$PROJECT_A/spio.lock" \
    >"$ROOT/reject.stdout" 2>"$ROOT/reject.stderr"; then
    fail "consumer accepted $file"
  fi
  grep -qx "resolution-v1:${code}" "$ROOT/reject.stderr" ||
    fail "consumer rejected $file for the wrong reason"
}

expect_reject "$ROOT/bad-schema.json" schema_version
expect_reject "$ROOT/bad-manifest.json" manifest_stale
expect_reject "$ROOT/bad-lock.json" lock_stale
expect_reject "$ROOT/bad-duplicate.json" package_ids
expect_reject "$ROOT/bad-dangling.json" dependency_reference
expect_reject "$ROOT/bad-root.json" package_root
expect_reject "$ROOT/bad-boundary.json" top_fields

# Contract source and target dependency direction remain package-only.
python3 - \
  "$REPO_ROOT/contracts/resolution-v1/resolution.schema.json" \
  "$REPO_ROOT/src/SpioResolve/ResolutionContract.hpp" \
  "$REPO_ROOT/src/SpioResolve/ResolutionContract.cpp" \
  "$REPO_ROOT/src/CMakeLists.txt" <<'PY'
import json
import pathlib
import re
import sys

schema = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
expected_top = {"schema_version", "manifest_sha256", "lock_sha256", "roots", "packages"}
if schema.get("type") != "object" or schema.get("additionalProperties") is not False:
    raise SystemExit("resolution schema must be a closed object")
if set(schema.get("properties", {})) != expected_top or set(schema.get("required", [])) != expected_top:
    raise SystemExit("resolution schema top-level fields are not minimal and exact")

for source_path in map(pathlib.Path, sys.argv[2:4]):
    includes = "\n".join(
        line for line in source_path.read_text(encoding="utf-8").splitlines()
        if line.lstrip().startswith("#include")
    )
    if re.search(r"Spio(?:Cloud|Tool|Toolchain|Plan)|WorkflowApp|ProjectGraphContract", includes):
        raise SystemExit(f"forbidden dependency in {source_path.name}")

cmake = pathlib.Path(sys.argv[4]).read_text(encoding="utf-8")
if "SpioResolve/ResolutionContract.cpp" not in cmake:
    raise SystemExit("resolution contract is not owned by spio_resolution")
match = re.search(
    r"target_link_libraries\(spio_resolution\s+PUBLIC\s+([^\)]*)\)",
    cmake,
    flags=re.MULTILINE,
)
if not match:
    raise SystemExit("spio_resolution link declaration is missing")
if re.search(r"cloud|tool|toolchain|plan|project", match.group(1), flags=re.IGNORECASE):
    raise SystemExit("spio_resolution links an out-of-bound service")

forbidden_fields = {
    "target",
    "profile",
    "toolchain",
    "cloud",
    "ide",
    "build_dir",
    "receipt",
    "command",
}


def walk(value):
    if isinstance(value, dict):
        for key, child in value.items():
            if key.lower() in forbidden_fields:
                raise SystemExit(f"forbidden schema field: {key}")
            walk(child)
    elif isinstance(value, list):
        for child in value:
            walk(child)


walk(schema)
PY

printf 'pafio-thin-core-contract: ok\n'
