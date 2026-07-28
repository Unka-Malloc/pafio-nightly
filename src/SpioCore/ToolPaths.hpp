#pragma once

#include <filesystem>
#include <string>

namespace spio
{

// Resolve curl/git/tar once to absolute paths (SPIO_CURL/SPIO_GIT/SPIO_TAR overrides).
// Security-relevant spawns must use these paths with search_path=false.
const std::string &ResolvedCurlPath();
const std::string &ResolvedGitPath();
const std::string &ResolvedTarPath();
const std::string &ResolvedOpenSslPath();

}  // namespace spio
