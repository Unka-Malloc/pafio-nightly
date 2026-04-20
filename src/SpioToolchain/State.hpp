#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace spio
{

struct ProjectToolchainState
{
  std::filesystem::path manifest_path;
  std::filesystem::path state_path;
  bool state_file_exists = false;
  std::string mode = "binary";
  std::string channel = "stable";
  std::string build_mode = "minimal";
  std::optional<std::string> source_revision;
};

struct ToolchainStateUpdate
{
  std::filesystem::path manifest_path = "spio.toml";
  std::optional<std::string> mode;
  std::optional<std::string> channel;
  std::optional<std::string> build_mode;
  std::optional<std::string> source_revision;
};

ProjectToolchainState LoadProjectToolchainState(const std::filesystem::path &manifest_path);
ProjectToolchainState UpdateProjectToolchainState(const ToolchainStateUpdate &update);

}  // namespace spio
