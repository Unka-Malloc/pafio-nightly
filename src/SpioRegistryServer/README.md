# Spio Registry Server

Owns registry write-side publication transports and origin-facing upload behavior.

The native open-source core now exposes a `RegistryHttpTransport` boundary so
remote publish orchestration can be tested and later upgraded to native async
HTTP without rewriting publish semantics.

Do not place client fetch/cache logic here.
Do not place auth tokens, account rules, or write-origin policy resolution here; those belong behind `src/SpioSecurity/` and optional `src-private/`.
