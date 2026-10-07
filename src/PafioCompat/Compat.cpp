#include "PafioCompat/Compat.hpp"

#include "PafioCore/Errors.hpp"
#include "PafioCore/Paths.hpp"
#include "PafioCore/Process.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>
#include <toml++/toml.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{

constexpr std::string_view kEmbeddedStyioSupportToml = R"toml(
schema = 1
pafio_version = "0.1.0-dev"

# Runtime admission requirements are independent of published product ranges.
# The row-local copies remain for older Pafio consumers of this matrix.
[runtime_requirements]
edition_max = "2026"
required_capabilities = ["machine_info_json", "single_file_entry", "jsonl_diagnostics"]
supported_compile_plan_versions = [1]
integration_phase = "compile-plan-live"

[[supported_styio]]
min = "0.0.1"
max_exclusive = "0.1.0"
channel = "stable"
edition_max = "2026"
required_capabilities = [
  "machine_info_json",
  "single_file_entry",
  "jsonl_diagnostics"
]
supported_compile_plan_versions = [1]
integration_phase = "compile-plan-live"
notes = "Current compatibility requires a styio compiler that advertises compile-plan v1 and accepts styio --compile-plan for build/run/test orchestration."

[[supported_styio]]
min = "0.0.1"
max_exclusive = "0.1.0"
channel = "nightly"
edition_max = "2026"
required_capabilities = [
  "machine_info_json",
  "single_file_entry",
  "jsonl_diagnostics"
]
supported_compile_plan_versions = [1]
integration_phase = "compile-plan-live"
notes = "The coordinated nightly lane consumes the same compile-plan v1 contract as the stable lane."
)toml";

int ParseNumber(const std::string &value, const std::string &field)
{
  int result = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (value.empty() || value.front() < '0' || value.front() > '9' ||
      parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < 0)
  {
    throw pafio::CompatibilityError("invalid " + field + ": " + value);
  }
  return result;
}

std::tuple<int, int, int> ParseSemver(const std::string &version)
{
  std::stringstream in(version);
  std::array<int, 3> parts{};
  for (int &part : parts)
  {
    std::string text;
    if (!std::getline(in, text, '.') || (text.size() > 1 && text.front() == '0'))
    {
      throw pafio::CompatibilityError("invalid compiler version in handshake: " + version);
    }
    part = ParseNumber(text, "compiler version in handshake");
  }
  if (!in.eof())
  {
    throw pafio::CompatibilityError("invalid compiler version in handshake: " + version);
  }
  return {parts[0], parts[1], parts[2]};
}

int ParseEdition(const std::string &edition)
{
  return ParseNumber(edition, "compiler edition_max in handshake");
}

toml::table LoadCompatMatrix()
{
  const fs::path compat_matrix_path = pafio::ProjectRoot() / "contracts" / "compat" / "styio-support.toml";
  if (fs::exists(compat_matrix_path))
  {
    try
    {
      return toml::parse_file(compat_matrix_path.string());
    }
    catch (const toml::parse_error &err)
    {
      throw pafio::CompatibilityError(
          "failed to parse compatibility matrix '" + compat_matrix_path.string() + "': " + std::string(err.description()));
    }
  }

  try
  {
    return toml::parse(kEmbeddedStyioSupportToml, std::string_view{"embedded styio-support.toml"});
  }
  catch (const toml::parse_error &err)
  {
    throw pafio::CompatibilityError("failed to parse embedded compatibility matrix: " + std::string(err.description()));
  }
}

json ProbeMachineInfo(const fs::path &binary)
{
  const pafio::ProcessResult result = pafio::RunProcess<pafio::CompilerProbeError>({
      .program = pafio::ProcessPathString(binary),
      .args = {"--machine-info=json"},
      .search_path = false,
      .timeout = pafio::kExternalProcessProbeTimeout,
      .error_context = "compiler probe process",
  });
  if (result.exit_code != 0)
  {
    const std::string detail = pafio::DescribeProcessFailure(result);
    throw pafio::CompilerProbeError("compiler '" + binary.string() + "' rejected --machine-info=json" + (detail.empty() ? "" : ": " + detail));
  }

  json payload;
  try
  {
    payload = json::parse(result.stdout_text);
  }
  catch (const json::parse_error &)
  {
    throw pafio::CompatibilityError("compiler '" + binary.string() + "' returned invalid machine-info JSON");
  }

  if (!payload.is_object())
  {
    throw pafio::CompatibilityError("compiler handshake must be an object");
  }
  for (const char *field : {"tool", "compiler_version", "channel", "edition_max"})
  {
    if (!payload.contains(field) || !payload[field].is_string() || payload[field].get<std::string>().empty())
    {
      throw pafio::CompatibilityError("compiler handshake field '" + std::string(field) + "' must be a non-empty string");
    }
  }
  if (payload["tool"] != "styio")
  {
    throw pafio::CompatibilityError("compiler handshake field 'tool' must equal 'styio'");
  }
  ParseSemver(payload["compiler_version"].get<std::string>());
  ParseEdition(payload["edition_max"].get<std::string>());
  if (!payload.contains("supported_contracts") || !payload["supported_contracts"].is_object())
  {
    throw pafio::CompatibilityError("compiler handshake field 'supported_contracts' must be an object");
  }
  const json &contracts = payload["supported_contracts"];
  if (!contracts.contains("compile_plan") || !contracts["compile_plan"].is_array() ||
      !std::all_of(contracts["compile_plan"].begin(), contracts["compile_plan"].end(), [](const json &value) {
        return value.is_number_integer() && value >= 0 && value <= INT32_MAX;
      }))
  {
    throw pafio::CompatibilityError("compiler handshake field 'supported_contracts.compile_plan' must be an array of non-negative integers");
  }
  if (!payload.contains("capabilities") || !payload["capabilities"].is_array() ||
      !std::all_of(payload["capabilities"].begin(), payload["capabilities"].end(), [](const json &value) { return value.is_string(); }))
  {
    throw pafio::CompatibilityError("compiler handshake field 'capabilities' must be an array of strings");
  }

  return payload;
}

const toml::table *FindMatchingEntry(const json &machine_info, const toml::table &compat_doc)
{
  const auto compiler_version = ParseSemver(machine_info["compiler_version"].get<std::string>());
  const std::string compiler_channel = machine_info["channel"].get<std::string>();

  const toml::array *entries = compat_doc["supported_styio"].as_array();
  if (entries == nullptr)
  {
    throw pafio::CompatibilityError("compatibility matrix is missing supported_styio entries");
  }

  for (const toml::node &node : *entries)
  {
    const toml::table *entry = node.as_table();
    if (entry == nullptr)
    {
      continue;
    }

    const auto min_version = ParseSemver(entry->at_path("min").value<std::string>().value_or(""));
    const auto max_version = ParseSemver(entry->at_path("max_exclusive").value<std::string>().value_or(""));
    const std::string channel = entry->at_path("channel").value<std::string>().value_or("");
    if (!(min_version <= compiler_version && compiler_version < max_version))
    {
      continue;
    }
    if (channel != compiler_channel)
    {
      continue;
    }
    return entry;
  }

  return nullptr;
}

std::vector<std::string> LoadStringArray(const toml::table &table, const std::string &key)
{
  std::vector<std::string> values;
  const toml::array *array = table[key].as_array();
  if (array == nullptr)
  {
    throw pafio::CompatibilityError("compatibility matrix field '" + key + "' must be an array");
  }

  values.reserve(array->size());
  for (const toml::node &node : *array)
  {
    const auto value = node.value<std::string>();
    if (!value.has_value())
    {
      throw pafio::CompatibilityError("invalid compatibility matrix array: " + key);
    }
    values.push_back(*value);
  }
  if (values.empty())
  {
    throw pafio::CompatibilityError("compatibility matrix array must not be empty: " + key);
  }
  return values;
}

std::vector<int> LoadIntArray(const toml::table &table, const std::string &key)
{
  std::vector<int> values;
  const toml::array *array = table[key].as_array();
  if (array == nullptr)
  {
    throw pafio::CompatibilityError("compatibility matrix field '" + key + "' must be an array");
  }

  values.reserve(array->size());
  for (const toml::node &node : *array)
  {
    const auto value = node.value<int64_t>();
    if (!value.has_value() || *value < 0 || *value > INT32_MAX)
    {
      throw pafio::CompatibilityError("invalid compatibility matrix array: " + key);
    }
    values.push_back(static_cast<int>(*value));
  }
  if (values.empty())
  {
    throw pafio::CompatibilityError("compatibility matrix array must not be empty: " + key);
  }
  return values;
}

bool IsExecutable(const fs::path &path)
{
#if defined(_WIN32)
  return _waccess(path.c_str(), 0) == 0;
#else
  return access(path.string().c_str(), X_OK) == 0;
#endif
}

std::optional<fs::path> FindStyioOnPath()
{
  const char *path_value = std::getenv("PATH");
  if (path_value == nullptr || *path_value == '\0')
  {
    return std::nullopt;
  }

#if defined(_WIN32)
  constexpr char kPathSeparator = ';';
  constexpr std::array<std::string_view, 2> kNames{"styio.exe", "styio"};
#else
  constexpr char kPathSeparator = ':';
  constexpr std::array<std::string_view, 1> kNames{"styio"};
#endif

  std::stringstream entries(path_value);
  std::string entry;
  while (std::getline(entries, entry, kPathSeparator))
  {
    if (entry.empty())
    {
      continue;
    }
    for (const std::string_view name : kNames)
    {
      const fs::path candidate = fs::path(entry) / name;
      if (fs::is_regular_file(candidate) && IsExecutable(candidate))
      {
        return pafio::CanonicalAbsolutePath(candidate);
      }
    }
  }
  return std::nullopt;
}

}  // namespace

namespace pafio
{

std::optional<ResolvedStyio> ResolveStyioBinary(
    const std::optional<std::string> &explicit_path)
{
  if (explicit_path.has_value())
  {
    if (explicit_path->empty())
    {
      throw CompilerProbeError("explicit Styio path must not be empty");
    }
    return ResolvedStyio{CanonicalAbsolutePath(*explicit_path), CompilerSelectionSource::CommandLine};
  }

  if (const char *env = std::getenv("PAFIO_STYIO_BIN"))
  {
    if (*env != '\0')
    {
      return ResolvedStyio{CanonicalAbsolutePath(env), CompilerSelectionSource::Environment};
    }
  }

  if (const auto binary = FindStyioOnPath())
  {
    return ResolvedStyio{*binary, CompilerSelectionSource::Path};
  }
  return std::nullopt;
}

CompatibilityReport CheckCompilerCompatibility(const fs::path &binary, CompilerSelectionSource source)
{
  const json machine_info = ProbeMachineInfo(binary);
  const toml::table compat_doc = LoadCompatMatrix();
  const toml::table *requirements = compat_doc["runtime_requirements"].as_table();
  if (requirements == nullptr)
  {
    throw CompatibilityError("compatibility matrix is missing runtime_requirements");
  }
  const toml::table &entry = *requirements;

  std::vector<std::string> capabilities = machine_info["capabilities"].get<std::vector<std::string>>();
  const std::vector<std::string> required_capabilities = LoadStringArray(entry, "required_capabilities");
  for (const std::string &required : required_capabilities)
  {
    if (std::find(capabilities.begin(), capabilities.end(), required) == capabilities.end())
    {
      throw CompatibilityError("compiler handshake is missing required capabilities: " + required);
    }
  }

  std::vector<int> supported_compile_plan_versions;
  if (machine_info["supported_contracts"].contains("compile_plan"))
  {
    supported_compile_plan_versions = machine_info["supported_contracts"]["compile_plan"].get<std::vector<int>>();
  }
  const std::vector<int> expected_compile_plan_versions = LoadIntArray(entry, "supported_compile_plan_versions");
  for (int version : expected_compile_plan_versions)
  {
    if (std::find(supported_compile_plan_versions.begin(), supported_compile_plan_versions.end(), version) == supported_compile_plan_versions.end())
    {
      throw CompatibilityError("compiler handshake does not provide the compile-plan versions required by this pafio phase");
    }
  }

  const int compiler_edition = ParseEdition(machine_info["edition_max"].get<std::string>());
  const int supported_edition = ParseEdition(entry["edition_max"].value<std::string>().value_or(""));
  if (compiler_edition < supported_edition)
  {
    throw CompatibilityError("compiler edition_max is lower than the minimum edition supported by this pafio phase");
  }

  // Runtime contracts always gate admission, even for a published product.
  const bool published_support = FindMatchingEntry(machine_info, compat_doc) != nullptr;
  if (!published_support && source == CompilerSelectionSource::Path)
  {
    throw CompatibilityError("compiler version/channel is outside the published pafio compatibility matrix; explicitly select a contract-compatible compiler with --styio-bin or PAFIO_STYIO_BIN");
  }
  std::sort(capabilities.begin(), capabilities.end());

  CompatibilityReport report;
  report.binary = fs::absolute(binary);
  report.compiler_version = machine_info["compiler_version"].get<std::string>();
  report.compiler_channel = machine_info["channel"].get<std::string>();
  report.compiler_edition_max = machine_info["edition_max"].get<std::string>();
  report.integration_phase = entry["integration_phase"].value<std::string>().value_or("unspecified");
  report.supported_compile_plan_versions = expected_compile_plan_versions;
  report.capabilities = std::move(capabilities);
  report.published_support = published_support;
  report.selection_source = source == CompilerSelectionSource::CommandLine ? "command_line" :
                            source == CompilerSelectionSource::Environment ? "environment" : "path";
  return report;
}

}  // namespace pafio
