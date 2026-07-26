# Spio Alpha Release Checklist

**Purpose:** Define the concrete closure checklist for publishing spio v0.1.0-alpha without implying npm-level package-manager maturity.

**Last updated:** 2026-07-11

**Authoritative execution graph:** `docs/plan/package-manager-roadmap/` and `docs/plan/repository-delivery-convergence/`

## Release Goal

The v0.1.0-alpha release is closed when a fresh supported machine can run:

```sh
curl -fsSL https://packages.styio.dev/tools/pafio/install-pafio.sh | sh -s -- --base-url https://packages.styio.dev && pafio install styio@latest && styio --version
```

`pafio doctor` remains the supported diagnostic command, but it is not required in the website one-liner.

The release is not expected to provide registry search, account management, complete semver range solving, Windows/mobile installers, or signed provenance.

## Required Closure

1. Publish real `pafio` prebuilt binaries under `tools/pafio/releases/<version>/<platform>/pafio`.
2. Publish real `styio` CLI prebuilts under target namespaces:
   - `tools/styio-linux/releases/<version>/<platform>/styio`
   - `tools/styio-macos-cli/releases/<version>/<platform>/styio`
3. Maintain shell-friendly channel pointers:
   - `tools/pafio/channel/latest/<platform>/version`
   - `tools/styio-linux/channel/stable/<platform>/version`
   - `tools/styio-macos-cli/channel/stable/<platform>/version`
4. Keep `latest.json` for API consumers, but do not require JSON parsing in the user-side installer.
5. Verify all published binaries with SHA-256 sidecar files.
6. Run `pafio doctor` on each supported target before announcing the release.
7. Run a clean-machine project workflow smoke:

```sh
pafio new local/hello hello
cd hello
pafio lock
pafio check
pafio build --dry-run
pafio vendor
pafio publish --dry-run
```

## Supported Alpha Targets

The installer adapter recognizes these Linux families:

- Ubuntu/Debian
- Fedora/CentOS Stream/Alma Linux/Rocky Linux/RHEL
- Arch Linux/Manjaro
- openSUSE
- Alpine Linux

Alpha support should be announced only for platform keys that actually have published binaries. The first local platform release closure set is:

- `darwin-aarch64`
- `linux-aarch64`
- `linux-musl-aarch64`

The x86_64 glibc and musl artifacts are deferred until CI or a preheated builder image can produce them repeatably.

## Known Alpha Constraints

- The Linux `styio` prebuilts are dynamically linked against the platform C++ runtime plus `zlib`, `zstd`, and, for glibc builds, `tinfo`. The installer adapter can identify distro families, but the alpha one-liner still assumes those normal runtime packages are present or installed by the operator.
- The macOS `styio` prebuilt currently links against Homebrew `zstd` through the LLVM 18 build used for release. A self-contained macOS CLI requires static zstd linkage or a multi-file archive installer.
- The musl build currently needs an Alpine LLVM 18 package workaround in the release builder because unused LLVM testing archive targets are declared by CMake metadata but not shipped by the package.
- Release builds still install roughly 1.2 GB of builder-side LLVM/Clang dependencies when the builder image is cold. This cost must move into CI image preparation before public release operations.

## Upgrade And Removal

The installer writes:

- `pafio` into the selected `--install-dir`, default `/usr/local/bin`
- `styio` shim into the same install directory unless `--no-styio-shim` is used
- `PAFIO_HOME/config/tool-release-root` when installed from a platform release root
- managed compilers under `PAFIO_HOME/tools/styio/`
- downloaded tool binaries under `PAFIO_HOME/cache/tool-releases/`

Manual removal for alpha is:

```sh
rm -f /usr/local/bin/pafio /usr/local/bin/styio
rm -rf "${PAFIO_HOME:-$HOME/.pafio}/tools/styio"
rm -rf "${PAFIO_HOME:-$HOME/.pafio}/cache/tool-releases"
```

Do not remove the whole `PAFIO_HOME` in a documented command unless the user also wants to discard registry cache, source cache, trust metadata, and server-side local registry state.

## Deferred Features

| Feature | Alpha stance | Estimated cost | Better Plan disposition |
|---------|--------------|----------------|-------------------------|
| Full semver range solver | Defer; support pinned/latest releases | Medium, touches resolver and lock semantics | **Closed in product scope:** `single-version-v1` exact pins are the contract (`docs/governance/Spio-Manifest-and-Lock-Conventions.md`, `docs/plan/package-manager-roadmap/Requirements.md` REQ-RES-001). Range solving requires a requirements update first. |
| Registry search and discovery | Defer; package roots must be explicit | Medium, needs index/query service and CLI UX | **Explicitly skipped** for alpha: tracked as a non-goal in `docs/plan/package-manager-roadmap/Requirements.md`; convergence node `35db9e15-6f48-4cce-83b0-4350d649152d` records the defer with rationale. Explicit `registry` roots in manifests and lockfiles remain required. |
| Private account auth | Defer to private security module and platform | Medium to high, needs tokens, policy, and audit | Tracked by convergence backlog; control-plane bearer auth covers publish mutation only. |
| Signed provenance | Defer; keep SHA-256 over HTTPS for alpha | Medium, needs signing keys, verification UX, CI custody | TUF chain verification covers registry consumption; signed provenance UX deferred. |
| Windows CLI installer | Defer unless real Windows artifacts exist | Medium, needs PowerShell installer and CI target | REQ-PLAT-001 records Windows scope decision. |
| Desktop/mobile package installation | Defer; platform target namespaces may exist first | High, separate packaging and app distribution paths | Out of alpha scope. |
| Cache garbage collection | Defer; document manual cleanup | Low to medium | Manual cleanup documented above. |
| Self-contained macOS/Linux archives | Defer past first alpha CLI smoke | Medium, needs archive metadata, RPATH/install-name handling, and runtime library policy | Known constraint section above. |
| x86_64 Linux artifacts | Defer until release builder exists | Low to medium, mostly CI capacity once build scripts are stable | Supported Alpha Targets section above. |
