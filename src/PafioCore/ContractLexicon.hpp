#pragma once

#include <string>
#include <string_view>

namespace pafio
{

// Lexical rules shared by pafio.toml, pafio.lock, dependency commands, and
// registry validation: package name (namespace/name), exact semver (x.y.z),
// and lowercase sha256 digests. Single authority — callers throw or test via
// these, never via private regex copies.
bool IsPackageName(const std::string &name);
bool IsExactSemver(const std::string &version);
bool IsSha256Digest(const std::string &digest);

void RequirePackageName(const std::string &name, std::string_view context);
void RequireExactSemver(const std::string &version, std::string_view context);
void RequireSha256Digest(const std::string &digest, std::string_view context);

}  // namespace pafio
