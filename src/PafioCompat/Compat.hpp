#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pafio
{

enum class CompilerSelectionSource
{
  CommandLine,
  Environment,
  Path,
};

struct ResolvedStyio
{
  std::filesystem::path binary;
  CompilerSelectionSource source = CompilerSelectionSource::Path;
};

struct CompatibilityReport
{
  std::filesystem::path binary;
  std::string compiler_version;
  std::string compiler_channel;
  std::string compiler_edition_max;
  std::string integration_phase;
  std::vector<int> supported_compile_plan_versions;
  std::vector<std::string> capabilities;
  std::string selection_source;
  // A published product-range match is advisory for an explicitly selected
  // compiler. A match does not certify release provenance.
  bool published_support = false;
};

std::optional<ResolvedStyio> ResolveStyioBinary(
    const std::optional<std::string> &explicit_path);
CompatibilityReport CheckCompilerCompatibility(
    const std::filesystem::path &binary,
    CompilerSelectionSource source = CompilerSelectionSource::Path);

}  // namespace pafio
