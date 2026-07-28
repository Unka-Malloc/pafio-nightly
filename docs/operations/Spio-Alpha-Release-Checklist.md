# Spio Alpha Release Checklist

**Purpose:** Define the concrete closure checklist for publishing `spio` v0.1.0-alpha without implying npm-level package-manager maturity.

**Last updated:** 2026-07-28

**Authoritative execution graph:** `docs/plan/package-manager-roadmap/` and
`docs/plan/repository-delivery-convergence/`

## Release Goal

The v0.1.0-alpha release is closed when a fresh supported machine can run:

```sh
curl -fsSL https://packages.styio.dev/tools/spio/install-spio.sh | sh -s -- --base-url https://packages.styio.dev && spio install styio@latest && styio --version
```

`spio doctor` remains the supported diagnostic command, but it is not required
in the website one-liner.

The release is not expected to provide registry search, account management,
complete semver range solving, mobile installers, or signed provenance.

## Required Closure

1. Publish real `spio` prebuilt binaries under `tools/spio/releases/<version>/<platform>/spio`, or `tools/spio/releases/<version>/<platform>/spio.exe` for Windows.
2. Publish real `styio` CLI prebuilts under target namespaces:
   - `tools/styio-linux/releases/<version>/<platform>/styio`
   - `tools/styio-macos-cli/releases/<version>/<platform>/styio`
   - `tools/styio-windows-cli/releases/<version>/<platform>/styio.exe`
3. Maintain shell-friendly channel pointers:
   - `tools/spio/channel/latest/<platform>/version`
   - `tools/styio-linux/channel/stable/<platform>/version`
   - `tools/styio-macos-cli/channel/stable/<platform>/version`
   - `tools/styio-windows-cli/channel/stable/<platform>/version`
4. Keep `latest.json` for API consumers, but do not require JSON parsing in the user-side installer.
5. Verify all published binaries with SHA-256 sidecar files.
6. Run `spio doctor` on each supported target before announcing the release.
7. Run a clean-machine project workflow smoke:

```sh
spio new local/hello hello
cd hello
spio lock
spio check
spio build --dry-run
spio vendor
spio publish --dry-run
```

## Supported Alpha Targets

The installer adapter recognizes these Linux families:

- Ubuntu/Debian
- Fedora/CentOS Stream/Alma Linux/Rocky Linux/RHEL
- Arch Linux/Manjaro
- openSUSE
- Alpine Linux

Alpha support should be announced only for platform keys that actually have
published binaries and clean-machine smoke evidence. The required platform
adaptation target set is:

- `darwin-aarch64`
- `linux-aarch64`
- `linux-musl-aarch64`
- `linux-x86_64` for Debian, Red Hat family, openSUSE, and Arch family glibc systems
- `linux-musl-x86_64` for Alpine and other supported musl x86_64 systems
- `windows-x86_64`

### macOS artifact and trust policy

- The alpha macOS payload is a raw Mach-O CLI at
  `tools/spio/releases/<version>/darwin-aarch64/spio`, accompanied by
  `spio.sha256`. The managed compiler uses the equivalent raw `styio` plus
  `styio.sha256` under the `styio-macos-cli` release root.
- The installer fetches over HTTPS, verifies the SHA-256 sidecar before
  promotion, and installs both launchers with mode `0755`. Multi-file
  `.tar.gz` or `.pkg` distribution is not part of the alpha layout.
- Alpha CLI artifacts are not code-signed or notarized. This must be stated on
  the download surface; Gatekeeper trust must not be implied.
- `curl` downloads normally do not receive a quarantine attribute. The
  installer refuses a downloaded payload carrying `com.apple.quarantine`
  rather than silently clearing it. For a separately transferred artifact,
  operators must verify its HTTPS origin and SHA-256 before explicitly
  removing that attribute.

The expanded target set is managed through the shared foundation plan and the
three platform-specific plans:

- [`../plan/spio-foundation/README.md`](../plan/spio-foundation/README.md)
- [`../plan/linux-compatibility/README.md`](../plan/linux-compatibility/README.md)
- [`../plan/macos-compatibility/README.md`](../plan/macos-compatibility/README.md)
- [`../plan/windows-compatibility/README.md`](../plan/windows-compatibility/README.md)

## Known Alpha Constraints

- The Linux `styio` prebuilts are dynamically linked against the platform C++
  runtime plus `zlib`, `zstd`, and, for glibc builds, `tinfo`. The installer
  adapter can identify distro families, but the alpha one-liner still assumes
  those normal runtime packages are present or installed by the operator.
- The macOS `styio` prebuilt currently links against Homebrew `zstd` through
  the LLVM 18 build used for release. A self-contained macOS CLI requires
  static zstd linkage or a multi-file archive installer.
- The musl build currently needs an Alpine LLVM 18 package workaround in the
  release builder because unused LLVM testing archive targets are declared by
  CMake metadata but not shipped by the package.
- Release builds still install roughly 1.2 GB of builder-side LLVM/Clang
  dependencies when the builder image is cold. This cost must move into CI
  image preparation before public release operations.

## Upgrade And Removal

The installer writes:

- `spio` into the selected `--install-dir`, default `/usr/local/bin`
- `styio` shim into the same install directory unless `--no-styio-shim` is used
- `SPIO_HOME/config/tool-release-root` when installed from a platform release root
- managed compilers under `SPIO_HOME/tools/styio/`
- downloaded tool binaries under `SPIO_HOME/cache/tool-releases/`

Manual removal for alpha is:

```sh
rm -f /usr/local/bin/spio /usr/local/bin/styio
rm -rf "${SPIO_HOME:-$HOME/.spio}/tools/styio"
rm -rf "${SPIO_HOME:-$HOME/.spio}/cache/tool-releases"
```

Do not remove the whole `SPIO_HOME` in a documented command unless the user also
wants to discard registry cache, source cache, trust metadata, and server-side
local registry state.

## Deferred Features

| Feature | Alpha stance | Estimated cost | Plan disposition |
|---------|--------------|----------------|------------------|
| Full semver range solver | Defer; support pinned/latest releases | Medium, touches resolver and lock semantics | `single-version-v1` exact pins remain the contract; range solving requires a requirements update. |
| Registry search and discovery | Defer; package roots must be explicit | Medium, needs index/query service and CLI UX | Explicitly excluded from alpha in the package-manager roadmap; manifest and lockfile registry roots remain required. |
| Private account auth | Defer to private security module and platform | Medium to high, needs tokens, policy, and audit | Tracked by the convergence backlog; control-plane bearer auth covers publish mutation only. |
| Signed provenance | Defer; keep SHA-256 over HTTPS for alpha | Medium, needs signing keys, verification UX, CI custody | TUF chain verification covers registry consumption; signed provenance UX remains deferred. |
| Windows CLI installer | Required for `windows-x86_64` support before announcement | Medium, needs PowerShell installer and real Windows smoke target | The platform roadmap keeps Windows unsupported until real artifacts and smoke evidence exist. |
| Desktop/mobile package installation | Defer; platform target namespaces may exist first | High, separate packaging and app distribution paths | Outside alpha scope. |
| Cache garbage collection | Defer; document manual cleanup | Low to medium | Manual cleanup is documented above. |
| Self-contained macOS/Linux archives | Defer past first alpha CLI smoke | Medium, needs archive metadata, RPATH/install-name handling, and runtime library policy | Remains an explicit known constraint. |
| x86_64 Linux artifacts | Required for Debian, Red Hat family, openSUSE, Arch family, and musl x86_64 support before announcement | Low to medium, mostly CI capacity once build scripts are stable | Required before those targets are announced; tracked by the platform roadmap. |
