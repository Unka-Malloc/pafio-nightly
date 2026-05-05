#pragma once

#include "SpioTool/Install.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace spio
{

struct ResolvedToolReleaseRoot
{
  std::string root;
  std::string source;
  bool explicit_root = false;
};

struct PrebuiltStyioInstallRequest
{
  ResolvedToolReleaseRoot release_root;
  std::string requested = "latest";
  std::string release_channel = "stable";
  std::optional<std::string> platform;
  std::optional<std::string> release_target;
};

struct PrebuiltStyioInstallResult
{
  ToolInstallResult install;
  std::string release_root;
  std::string release_root_source;
  std::string release_channel;
  std::string release_version;
  std::string release_target;
  std::string platform;
  std::string binary_url;
  std::string sha256_url;
  std::string sha256;
  std::filesystem::path downloaded_binary_path;
};

std::optional<ResolvedToolReleaseRoot> ResolveStyioToolReleaseRoot(
    const std::optional<std::string> &explicit_release_root);
std::string DetectToolReleasePlatform();
std::string DetectStyioClientReleaseTarget(const std::string &platform);
PrebuiltStyioInstallResult InstallPrebuiltStyio(const PrebuiltStyioInstallRequest &request);

}  // namespace spio
