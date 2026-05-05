#!/usr/bin/env sh
set -eu

usage() {
  cat <<'USAGE'
Usage: install-spio.sh [options]

Install a prebuilt spio binary from an HTTP(S) release root. This script is safe
for a curl pipeline:

  curl -fsSL https://packages.example.invalid/tools/spio/install-spio.sh | sh -s -- --base-url https://packages.example.invalid

Legacy direct-binary layout is still supported:

  curl -fsSL https://example.invalid/spio/install-spio.sh | sh -s -- --base-url https://example.invalid/spio

Options:
  --base-url <url>      Release root. The installer first tries
                        <url>/tools/spio/channel/<channel>/<platform>/version,
                        then falls back to <url>/spio for legacy layouts.
  --channel <name>      Release channel to install (default: latest).
  --version <value>     Exact release version. Skips channel lookup.
  --binary-url <url>    Exact URL for the spio binary. Bypasses release lookup.
  --sha256-url <url>    Exact URL for the expected sha256 text.
  --platform <value>    Platform key to install. Defaults to uname detection.
  --install-dir <dir>   Install directory (default: /usr/local/bin).
  --binary-name <name>  Installed executable name (default: spio).
  --no-styio-shim       Do not install the companion styio shim.
  --no-release-root-config
                        Do not write SPIO_HOME/config/tool-release-root.
  --print-platform      Print the detected release platform and exit.
  --print-adapter       Print the detected Linux distro adapter and exit.
  -h, --help            Show this help.
USAGE
}

fail() {
  echo "install-spio: $*" >&2
  exit 1
}

lowercase() {
  printf '%s\n' "$1" | tr '[:upper:]' '[:lower:]'
}

uname_s() {
  if [ -n "${SPIO_INSTALL_UNAME_S:-}" ]; then
    printf '%s\n' "$SPIO_INSTALL_UNAME_S"
  else
    uname -s
  fi
}

uname_m() {
  if [ -n "${SPIO_INSTALL_UNAME_M:-}" ]; then
    printf '%s\n' "$SPIO_INSTALL_UNAME_M"
  else
    uname -m
  fi
}

os_release_value() {
  file="$1"
  key="$2"
  [ -r "$file" ] || return 1
  value="$(grep "^$key=" "$file" 2>/dev/null | head -n 1 | cut -d= -f2- || true)"
  [ -n "$value" ] || return 1
  case "$value" in
    \"*\")
      value="${value#\"}"
      value="${value%\"}"
      ;;
    \'*\')
      value="${value#\'}"
      value="${value%\'}"
      ;;
  esac
  printf '%s\n' "$value"
}

detect_distro_family() {
  os_release_file="${SPIO_INSTALL_OS_RELEASE_FILE:-/etc/os-release}"
  distro_id="$(os_release_value "$os_release_file" ID 2>/dev/null || true)"
  distro_like="$(os_release_value "$os_release_file" ID_LIKE 2>/dev/null || true)"
  tokens="$(printf ' %s %s ' "$distro_id" "$distro_like" | tr '[:upper:]' '[:lower:]' | tr '\n' ' ')"

  case "$tokens" in
    *" alpine "*)
      echo "alpine"
      ;;
    *" debian "*|*" ubuntu "*)
      echo "debian"
      ;;
    *" fedora "*|*" rhel "*|*" centos "*|*" rocky "*|*" almalinux "*)
      echo "redhat"
      ;;
    *" arch "*|*" manjaro "*)
      echo "arch"
      ;;
    *" opensuse"*|*" suse "*|*" sles "*)
      echo "opensuse"
      ;;
    *)
      echo "unknown"
      ;;
  esac
}

detect_linux_libc() {
  if [ -n "${SPIO_INSTALL_LIBC:-}" ]; then
    libc_override="$(lowercase "$SPIO_INSTALL_LIBC")"
    case "$libc_override" in
      glibc|musl)
        echo "$libc_override"
        return 0
        ;;
      *)
        fail "unsupported SPIO_INSTALL_LIBC value: $SPIO_INSTALL_LIBC"
        ;;
    esac
  fi

  if [ "$(detect_distro_family)" = "alpine" ] || [ -e /etc/alpine-release ]; then
    echo "musl"
    return 0
  fi

  if command -v getconf >/dev/null 2>&1 && getconf GNU_LIBC_VERSION >/dev/null 2>&1; then
    echo "glibc"
    return 0
  fi

  if command -v ldd >/dev/null 2>&1; then
    ldd_output="$(ldd --version 2>&1 || true)"
    if printf '%s\n' "$ldd_output" | grep -qi 'musl'; then
      echo "musl"
      return 0
    fi
    if printf '%s\n' "$ldd_output" | grep -Eqi 'glibc|gnu libc|gnu c library'; then
      echo "glibc"
      return 0
    fi
  fi

  for loader in \
    /lib/ld-musl-aarch64.so.1 \
    /lib/ld-musl-x86_64.so.1 \
    /lib/ld-musl-armhf.so.1 \
    /lib/ld-musl-armv7.so.1
  do
    if [ -e "$loader" ]; then
      echo "musl"
      return 0
    fi
  done

  echo "glibc"
}

hint_prefix() {
  uid="$(id -u 2>/dev/null || printf '1')"
  if [ "$uid" = "0" ]; then
    printf ''
  else
    printf 'sudo '
  fi
}

distro_package_manager() {
  case "${1:-$(detect_distro_family)}" in
    debian) echo "apt-get" ;;
    redhat) echo "dnf" ;;
    arch) echo "pacman" ;;
    opensuse) echo "zypper" ;;
    alpine) echo "apk" ;;
    *) echo "unknown" ;;
  esac
}

distro_prerequisite_command() {
  family="${1:-$(detect_distro_family)}"
  prefix="$(hint_prefix)"
  case "$family" in
    debian)
      printf '%sapt-get update && %sapt-get install -y ca-certificates curl coreutils\n' "$prefix" "$prefix"
      ;;
    redhat)
      printf '%sdnf install -y ca-certificates curl-minimal coreutils\n' "$prefix"
      ;;
    arch)
      printf '%spacman -Sy --needed ca-certificates curl coreutils\n' "$prefix"
      ;;
    opensuse)
      printf '%szypper --non-interactive install ca-certificates curl coreutils\n' "$prefix"
      ;;
    alpine)
      printf '%sapk add --no-cache ca-certificates curl coreutils libstdc++\n' "$prefix"
      ;;
    *)
      return 1
      ;;
  esac
}

missing_command_message() {
  command_name="$1"
  family="$(detect_distro_family)"
  hint="$(distro_prerequisite_command "$family" 2>/dev/null || true)"
  if [ -n "$hint" ]; then
    printf "%s is required. Supported distro family '%s' can install prerequisites with: %s\n" "$command_name" "$family" "$hint"
  else
    printf "%s is required. Install ca-certificates, curl, and coreutils with your system package manager.\n" "$command_name"
  fi
}

require_command() {
  command_name="$1"
  if ! command -v "$command_name" >/dev/null 2>&1; then
    fail "$(missing_command_message "$command_name")"
  fi
}

install_dir_is_writable() {
  dir="$1"
  if [ -d "$dir" ]; then
    [ -w "$dir" ]
    return $?
  fi
  parent="$(dirname "$dir")"
  [ -d "$parent" ] && [ -w "$parent" ]
}

BASE_URL="${SPIO_INSTALL_BASE_URL:-}"
BINARY_URL="${SPIO_INSTALL_BINARY_URL:-}"
SHA256_URL="${SPIO_INSTALL_SHA256_URL:-}"
CHANNEL="${SPIO_INSTALL_CHANNEL:-latest}"
RELEASE_VERSION="${SPIO_INSTALL_VERSION:-}"
PLATFORM="${SPIO_INSTALL_PLATFORM:-}"
INSTALL_DIR="${SPIO_INSTALL_DIR:-/usr/local/bin}"
BINARY_NAME="${SPIO_INSTALL_BINARY_NAME:-spio}"
INSTALL_STYIO_SHIM=1
WRITE_RELEASE_ROOT_CONFIG=1
SPIO_HOME_DIR="${SPIO_HOME:-$HOME/.spio}"
EXPECTED_SHA256=""
CHANNEL_EXPLICIT=0
VERSION_EXPLICIT=0
RELEASE_ROOT_CONFIG_URL=""
PRINT_PLATFORM=0
PRINT_ADAPTER=0

detect_platform() {
  os="$(uname_s | tr '[:upper:]' '[:lower:]')"
  arch="$(uname_m | tr '[:upper:]' '[:lower:]')"

  case "$os" in
    linux)
      if [ "$(detect_linux_libc)" = "musl" ]; then
        os="linux-musl"
      else
        os="linux"
      fi
      ;;
    darwin)
      os="darwin"
      ;;
    *)
      fail "unsupported OS for automatic platform detection: $(uname_s)"
      ;;
  esac

  case "$arch" in
    aarch64|arm64)
      arch="aarch64"
      ;;
    x86_64|amd64)
      arch="x86_64"
      ;;
    *)
      fail "unsupported CPU for automatic platform detection: $(uname_m)"
      ;;
  esac

  echo "$os-$arch"
}

is_musl_platform() {
  case "$1" in
    linux-musl-*) return 0 ;;
    *) return 1 ;;
  esac
}

print_adapter() {
  family="$(detect_distro_family)"
  libc=""
  if [ "$(lowercase "$(uname_s)")" = "linux" ]; then
    libc="$(detect_linux_libc)"
  fi
  platform="$(detect_platform)"
  manager="$(distro_package_manager "$family")"
  hint="$(distro_prerequisite_command "$family" 2>/dev/null || true)"

  printf 'distro_family=%s\n' "$family"
  printf 'package_manager=%s\n' "$manager"
  if [ -n "$libc" ]; then
    printf 'libc=%s\n' "$libc"
  fi
  printf 'platform=%s\n' "$platform"
  if [ -n "$hint" ]; then
    printf 'prerequisite_command=%s\n' "$hint"
  fi
}

sha256_value() {
  file="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
    return 0
  fi
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file" | awk '{print $1}'
    return 0
  fi
  fail "$(missing_command_message "sha256sum or shasum")"
}

verify_sha256() {
  file="$1"
  expected="$2"
  actual="$(sha256_value "$file")"
  if [ "$actual" != "$expected" ]; then
    fail "sha256 mismatch for downloaded spio binary: expected $expected, got $actual"
  fi
}

download_text_first_field() {
  url="$1"
  stderr_file="$2"
  output_file="$TMP_DIR/text-download.$$"
  value=""
  if ! curl -fsSL "$url" -o "$output_file" 2>"$stderr_file"; then
    rm -f "$output_file"
    return 1
  fi
  value="$(awk 'NF { print $1; exit }' "$output_file")"
  rm -f "$output_file"
  [ -n "$value" ] || return 1
  printf '%s\n' "$value"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --base-url)
      [ "$#" -ge 2 ] || fail "--base-url requires a value"
      BASE_URL="$2"
      shift 2
      ;;
    --channel)
      [ "$#" -ge 2 ] || fail "--channel requires a value"
      CHANNEL="$2"
      CHANNEL_EXPLICIT=1
      shift 2
      ;;
    --version)
      [ "$#" -ge 2 ] || fail "--version requires a value"
      RELEASE_VERSION="$2"
      VERSION_EXPLICIT=1
      shift 2
      ;;
    --binary-url)
      [ "$#" -ge 2 ] || fail "--binary-url requires a value"
      BINARY_URL="$2"
      shift 2
      ;;
    --sha256-url)
      [ "$#" -ge 2 ] || fail "--sha256-url requires a value"
      SHA256_URL="$2"
      shift 2
      ;;
    --platform)
      [ "$#" -ge 2 ] || fail "--platform requires a value"
      PLATFORM="$2"
      shift 2
      ;;
    --install-dir)
      [ "$#" -ge 2 ] || fail "--install-dir requires a value"
      INSTALL_DIR="$2"
      shift 2
      ;;
    --binary-name)
      [ "$#" -ge 2 ] || fail "--binary-name requires a value"
      BINARY_NAME="$2"
      shift 2
      ;;
    --no-styio-shim)
      INSTALL_STYIO_SHIM=0
      shift
      ;;
    --no-release-root-config)
      WRITE_RELEASE_ROOT_CONFIG=0
      shift
      ;;
    --print-platform)
      PRINT_PLATFORM=1
      shift
      ;;
    --print-adapter)
      PRINT_ADAPTER=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "unknown option: $1"
      ;;
  esac
done

if [ "$PRINT_PLATFORM" -eq 1 ]; then
  detect_platform
  exit 0
fi

if [ "$PRINT_ADAPTER" -eq 1 ]; then
  print_adapter
  exit 0
fi

require_command curl
require_command install

TMP_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

if [ -z "$BINARY_URL" ]; then
  [ -n "$BASE_URL" ] || fail "pass --base-url or --binary-url"
  if [ -z "$PLATFORM" ]; then
    PLATFORM="$(detect_platform)"
  fi
  BASE_URL="${BASE_URL%/}"

  if [ -z "$RELEASE_VERSION" ]; then
    CHANNEL_VERSION_URL="$BASE_URL/tools/spio/channel/$CHANNEL/$PLATFORM/version"
    if [ "$CHANNEL_EXPLICIT" -eq 1 ]; then
      CHANNEL_CURL_STDERR="$TMP_DIR/channel-curl.stderr"
    else
      CHANNEL_CURL_STDERR="/dev/null"
    fi
    if RELEASE_VERSION="$(download_text_first_field "$CHANNEL_VERSION_URL" "$CHANNEL_CURL_STDERR")" &&
       [ -n "$RELEASE_VERSION" ]; then
      :
    elif [ "$CHANNEL_EXPLICIT" -eq 1 ] || is_musl_platform "$PLATFORM"; then
      if [ "$CHANNEL_CURL_STDERR" != "/dev/null" ]; then
        cat "$CHANNEL_CURL_STDERR" >&2 || true
      fi
      fail "failed to resolve spio channel '$CHANNEL' for platform '$PLATFORM': $CHANNEL_VERSION_URL"
    else
      BINARY_URL="$BASE_URL/spio"
    fi
  fi

  if [ -n "$RELEASE_VERSION" ] && [ -z "$BINARY_URL" ]; then
    BINARY_URL="$BASE_URL/tools/spio/releases/$RELEASE_VERSION/$PLATFORM/spio"
    RELEASE_ROOT_CONFIG_URL="$BASE_URL"
    if [ -z "$SHA256_URL" ]; then
      SHA256_URL="$BINARY_URL.sha256"
    fi
  fi
elif [ "$VERSION_EXPLICIT" -eq 1 ] || [ "$CHANNEL_EXPLICIT" -eq 1 ]; then
  fail "--version and --channel are only valid with --base-url release lookup"
fi

TMP_BIN="$TMP_DIR/spio"
TMP_STYIO_SHIM="$TMP_DIR/styio"
curl -fsSL "$BINARY_URL" -o "$TMP_BIN"
if [ -n "$SHA256_URL" ]; then
  SHA256_CURL_STDERR="$TMP_DIR/sha256-curl.stderr"
  EXPECTED_SHA256="$(download_text_first_field "$SHA256_URL" "$SHA256_CURL_STDERR")" || {
    cat "$SHA256_CURL_STDERR" >&2 || true
    fail "failed to fetch sha256: $SHA256_URL"
  }
fi
if [ -n "$EXPECTED_SHA256" ]; then
  verify_sha256 "$TMP_BIN" "$EXPECTED_SHA256"
fi
chmod 0755 "$TMP_BIN"
cat >"$TMP_STYIO_SHIM" <<'EOF'
#!/usr/bin/env sh
set -eu
SPIO_HOME_DIR="${SPIO_HOME:-$HOME/.spio}"
STYIO_BIN="$SPIO_HOME_DIR/tools/styio/current/bin/styio"
if [ ! -x "$STYIO_BIN" ]; then
  echo "styio is not installed; run: spio install styio@latest" >&2
  exit 127
fi
exec "$STYIO_BIN" "$@"
EOF
chmod 0755 "$TMP_STYIO_SHIM"

if install_dir_is_writable "$INSTALL_DIR"; then
  mkdir -p "$INSTALL_DIR"
  install -m 0755 "$TMP_BIN" "$INSTALL_DIR/$BINARY_NAME"
  if [ "$INSTALL_STYIO_SHIM" -eq 1 ]; then
    install -m 0755 "$TMP_STYIO_SHIM" "$INSTALL_DIR/styio"
  fi
elif command -v sudo >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
  sudo install -d -m 0755 "$INSTALL_DIR"
  sudo install -m 0755 "$TMP_BIN" "$INSTALL_DIR/$BINARY_NAME"
  if [ "$INSTALL_STYIO_SHIM" -eq 1 ]; then
    sudo install -m 0755 "$TMP_STYIO_SHIM" "$INSTALL_DIR/styio"
  fi
else
  fail "$INSTALL_DIR is not writable and passwordless sudo is unavailable; pass --install-dir \$HOME/.local/bin"
fi

if [ "$WRITE_RELEASE_ROOT_CONFIG" -eq 1 ] && [ -n "$RELEASE_ROOT_CONFIG_URL" ]; then
  mkdir -p "$SPIO_HOME_DIR/config"
  printf '%s\n' "$RELEASE_ROOT_CONFIG_URL" >"$SPIO_HOME_DIR/config/tool-release-root"
fi

if command -v "$BINARY_NAME" >/dev/null 2>&1; then
  "$BINARY_NAME" --version
else
  echo "installed $INSTALL_DIR/$BINARY_NAME"
  if [ -n "$RELEASE_VERSION" ]; then
    echo "installed spio release $RELEASE_VERSION for ${PLATFORM:-unknown-platform}"
  fi
  echo "add $INSTALL_DIR to PATH before running $BINARY_NAME"
fi
