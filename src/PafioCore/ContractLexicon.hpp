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

// Git revision tokens name cache directories and git argv: they must stay a
// single safe path component (no '.', '..', or '/' traversal, no leading '-')
// while still accepting shas, tags, branch names, and revspec suffixes.
bool IsGitRevisionToken(const std::string &rev);

// Registry roots are cache keys and trust inputs: one normalization (strip
// trailing '/') shared by the client, verifier, resolver, and publish path.
std::string NormalizeRegistryRoot(std::string value);

// The short name is the segment after '/' in a validated namespace/name.
std::string PackageShortName(const std::string &package_name);

void RequirePackageName(const std::string &name, std::string_view context);
void RequireExactSemver(const std::string &version, std::string_view context);
void RequireSha256Digest(const std::string &digest, std::string_view context);
void RequireGitRevisionToken(const std::string &rev, std::string_view context);

}  // namespace pafio
