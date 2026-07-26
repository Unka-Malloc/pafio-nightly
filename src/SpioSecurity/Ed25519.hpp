#pragma once

#include <string>
#include <string_view>

namespace spio
{

// Ed25519 verify primitive shared by TufVerifier and RegistryTrust.
// Decision (Architecture.md): call OpenSSL `pkeyutl -verify` over canonical
// message bytes, matching src/spio_registry_v2/common.py::verify_signature,
// rather than linking a TLS library into the native tree.
bool Ed25519VerifyPem(std::string_view public_key_pem, std::string_view message, std::string_view signature_b64);

}  // namespace spio
