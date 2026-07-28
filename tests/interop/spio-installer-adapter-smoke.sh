#!/usr/bin/env bash
set -euo pipefail

die() {
  echo "spio-installer-adapter-smoke: $*" >&2
  exit 1
}

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "$script_dir/../.." && pwd)
installer="$repo_root/scripts/install-spio.sh"

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/spio-installer-adapter.XXXXXX")
cleanup() {
  rm -rf "$work_dir"
}
trap cleanup EXIT

run_adapter() {
  name="$1"
  os_release="$2"
  arch="${3:-aarch64}"
  libc="${4:-glibc}"
  os_release_file="$work_dir/$name.os-release"
  printf '%s\n' "$os_release" >"$os_release_file"
  SPIO_INSTALL_OS_RELEASE_FILE="$os_release_file" \
    SPIO_INSTALL_UNAME_S=Linux \
    SPIO_INSTALL_UNAME_M="$arch" \
    SPIO_INSTALL_LIBC="$libc" \
    sh "$installer" --print-adapter
}

assert_contains() {
  text="$1"
  expected="$2"
  printf '%s\n' "$text" | grep -Fq "$expected" || die "expected adapter output to contain '$expected', got: $text"
}

ubuntu_output=$(run_adapter ubuntu 'ID=ubuntu
ID_LIKE=debian' x86_64)
assert_contains "$ubuntu_output" 'distro_family=debian'
assert_contains "$ubuntu_output" 'package_manager=apt-get'
assert_contains "$ubuntu_output" 'platform=linux-x86_64'
assert_contains "$ubuntu_output" 'apt-get install -y ca-certificates curl coreutils'

fedora_output=$(run_adapter fedora 'ID=fedora' x86_64)
assert_contains "$fedora_output" 'distro_family=redhat'
assert_contains "$fedora_output" 'package_manager=dnf'
assert_contains "$fedora_output" 'platform=linux-x86_64'
assert_contains "$fedora_output" 'dnf install -y ca-certificates curl-minimal coreutils'

rhel_output=$(run_adapter rhel 'ID=rhel
ID_LIKE="fedora"' x86_64)
assert_contains "$rhel_output" 'distro_family=redhat'
assert_contains "$rhel_output" 'package_manager=dnf'
assert_contains "$rhel_output" 'platform=linux-x86_64'

manjaro_output=$(run_adapter manjaro 'ID=manjaro
ID_LIKE=arch' x86_64)
assert_contains "$manjaro_output" 'distro_family=arch'
assert_contains "$manjaro_output" 'package_manager=pacman'
assert_contains "$manjaro_output" 'platform=linux-x86_64'
assert_contains "$manjaro_output" 'pacman -Sy --needed ca-certificates curl coreutils'

opensuse_output=$(run_adapter opensuse 'ID=opensuse-leap
ID_LIKE="suse opensuse"' x86_64)
assert_contains "$opensuse_output" 'distro_family=opensuse'
assert_contains "$opensuse_output" 'package_manager=zypper'
assert_contains "$opensuse_output" 'platform=linux-x86_64'
assert_contains "$opensuse_output" 'zypper --non-interactive install ca-certificates curl coreutils'

alpine_output=$(run_adapter alpine 'ID=alpine' x86_64 musl)
assert_contains "$alpine_output" 'distro_family=alpine'
assert_contains "$alpine_output" 'package_manager=apk'
assert_contains "$alpine_output" 'libc=musl'
assert_contains "$alpine_output" 'platform=linux-musl-x86_64'
assert_contains "$alpine_output" 'apk add --no-cache ca-certificates curl coreutils libstdc++'

macos_output=$(
  SPIO_INSTALL_UNAME_S=Darwin \
    SPIO_INSTALL_UNAME_M=arm64 \
    sh "$installer" --print-adapter
)
assert_contains "$macos_output" 'distro_family=macos'
assert_contains "$macos_output" 'package_manager=system'
assert_contains "$macos_output" 'platform=darwin-aarch64'
assert_contains "$macos_output" 'xcode-select --install'

echo "spio installer adapter smoke passed"
