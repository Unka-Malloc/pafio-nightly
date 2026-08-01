#pragma once

#include <filesystem>
#include <string>

namespace pafio
{

// Resolve curl/git/tar once to absolute paths (PAFIO_CURL/PAFIO_GIT/PAFIO_TAR overrides).
// Security-relevant spawns must use these paths with search_path=false.
const std::string &ResolvedCurlPath();
const std::string &ResolvedGitPath();
const std::string &ResolvedTarPath();
const std::string &ResolvedOpenSslPath();

}  // namespace pafio
