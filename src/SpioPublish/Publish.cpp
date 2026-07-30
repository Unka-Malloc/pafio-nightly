#include "SpioPublish/Publish.hpp"

#include "SpioCore/Errors.hpp"
#include "SpioCore/Sha256.hpp"
#include "SpioManifest/Manifest.hpp"
#include "SpioPack/Pack.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

constexpr size_t kMaxRemotePublishDependenciesPerTable = 256U;
constexpr size_t kMaxRemotePublishPackageBytes = 255U;
constexpr size_t kMaxRemotePublishVersionBytes = 64U;
constexpr size_t kMaxRemotePublishAliasBytes = 128U;
constexpr size_t kMaxRemotePublishRegistryBytes = 2048U;
constexpr size_t kMaxRemotePublishArchiveNameBytes = 255U;

struct PackageSelection
{
  fs::path manifest_path;
  fs::path package_root;
  spio::ManifestDocument manifest;
};

fs::path CanonicalAbsolutePath(const fs::path &path)
{
  return fs::absolute(path).lexically_normal();
}

std::vector<PackageSelection> CollectPackageCandidates(const fs::path &root_manifest_path)
{
  const fs::path normalized_root_manifest = CanonicalAbsolutePath(root_manifest_path);
  const fs::path root_dir = normalized_root_manifest.parent_path();
  const spio::ManifestDocument root_manifest = spio::LoadManifest(normalized_root_manifest);

  std::vector<PackageSelection> candidates;
  if (root_manifest.package.has_value())
  {
    candidates.push_back({
        .manifest_path = normalized_root_manifest,
        .package_root = root_dir,
        .manifest = root_manifest,
    });
  }

  if (root_manifest.workspace.has_value())
  {
    for (const std::string &member : root_manifest.workspace->members)
    {
      const fs::path member_manifest_path = CanonicalAbsolutePath(root_dir / member / "spio.toml");
      const spio::ManifestDocument member_manifest = spio::LoadManifest(member_manifest_path);
      if (!member_manifest.package.has_value())
      {
        throw spio::WorkspaceError("workspace member manifest must define [package]: " + member_manifest_path.string());
      }
      candidates.push_back({
          .manifest_path = member_manifest_path,
          .package_root = member_manifest_path.parent_path(),
          .manifest = member_manifest,
      });
    }
  }

  return candidates;
}

PackageSelection SelectPackageForPublish(const spio::PublishRequest &request)
{
  const fs::path root_manifest_path = CanonicalAbsolutePath(request.manifest_path);
  const std::vector<PackageSelection> candidates = CollectPackageCandidates(root_manifest_path);
  if (candidates.empty())
  {
    throw spio::PublishError("publish requires a manifest with a local [package] target");
  }

  if (request.package_name.has_value())
  {
    std::vector<const PackageSelection *> matches;
    for (const PackageSelection &candidate : candidates)
    {
      if (candidate.manifest.package->name == *request.package_name)
      {
        matches.push_back(&candidate);
      }
    }
    if (matches.empty())
    {
      throw spio::PublishError("selected package was not found under the active manifest: " + *request.package_name);
    }
    if (matches.size() > 1U)
    {
      throw spio::PublishError("selected package is ambiguous under the active manifest: " + *request.package_name);
    }
    return *matches.front();
  }

  for (const PackageSelection &candidate : candidates)
  {
    if (candidate.manifest_path == root_manifest_path && candidate.manifest.package.has_value())
    {
      return candidate;
    }
  }

  if (candidates.size() == 1U)
  {
    return candidates.front();
  }

  throw spio::PublishError("workspace publish is ambiguous; select a root package with --package <namespace/name>");
}

void ValidatePublishCandidate(const PackageSelection &selection)
{
  const spio::PackageConfig &package = *selection.manifest.package;
  if (!package.publish)
  {
    throw spio::PublishError("selected package is marked publish = false: " + package.name);
  }

  auto validate_dependency_table = [&](const std::vector<spio::Dependency> &dependencies, const std::string &section_name) {
    for (const spio::Dependency &dependency : dependencies)
    {
      if (dependency.source_kind != spio::DependencySourceKind::kRegistry)
      {
        throw spio::PublishError(
            "published packages may only contain registry-addressable dependencies in [" + section_name + "]: " +
            package.name);
      }
      if (!dependency.package.has_value() || dependency.package->empty())
      {
        throw spio::PublishError(
            "registry dependency in [" + section_name + "] must include package = \"namespace/name\": " + package.name);
      }
      if (!dependency.version.has_value() || dependency.version->empty())
      {
        throw spio::PublishError(
            "registry dependency in [" + section_name + "] must include version = \"x.y.z\": " + package.name);
      }
      if (dependency.source.empty())
      {
        throw spio::PublishError(
            "registry dependency in [" + section_name + "] must include registry = \"<url>\": " + package.name);
      }
    }
  };

  validate_dependency_table(package.dependencies, "dependencies");
  validate_dependency_table(package.dev_dependencies, "dev-dependencies");
}

std::string EncodeBase64(const std::string &bytes)
{
  constexpr std::array<char, 64> kAlphabet = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/',
  };

  std::string encoded;
  encoded.reserve(((bytes.size() + 2U) / 3U) * 4U);

  size_t offset = 0;
  while (offset + 3U <= bytes.size())
  {
    const auto first = static_cast<unsigned char>(bytes[offset]);
    const auto second = static_cast<unsigned char>(bytes[offset + 1U]);
    const auto third = static_cast<unsigned char>(bytes[offset + 2U]);
    encoded.push_back(kAlphabet[first >> 2U]);
    encoded.push_back(kAlphabet[((first & 0x03U) << 4U) | (second >> 4U)]);
    encoded.push_back(kAlphabet[((second & 0x0fU) << 2U) | (third >> 6U)]);
    encoded.push_back(kAlphabet[third & 0x3fU]);
    offset += 3U;
  }

  const size_t remaining = bytes.size() - offset;
  if (remaining == 1U)
  {
    const auto first = static_cast<unsigned char>(bytes[offset]);
    encoded.push_back(kAlphabet[first >> 2U]);
    encoded.push_back(kAlphabet[(first & 0x03U) << 4U]);
    encoded.append("==");
  }
  else if (remaining == 2U)
  {
    const auto first = static_cast<unsigned char>(bytes[offset]);
    const auto second = static_cast<unsigned char>(bytes[offset + 1U]);
    encoded.push_back(kAlphabet[first >> 2U]);
    encoded.push_back(kAlphabet[((first & 0x03U) << 4U) | (second >> 4U)]);
    encoded.push_back(kAlphabet[(second & 0x0fU) << 2U]);
    encoded.push_back('=');
  }

  return encoded;
}

void ValidateBoundedPublishString(
    const std::string &value,
    const std::string &field_name,
    const size_t max_size_bytes)
{
  if (value.empty())
  {
    throw spio::PublishError("remote publish field must not be empty: " + field_name);
  }
  if (value.size() > max_size_bytes)
  {
    throw spio::PublishError(
        "remote publish field exceeds the " + std::to_string(max_size_bytes) + "-byte limit: " + field_name);
  }
}

nlohmann::json SerializeRemotePublishDependencies(
    const std::vector<spio::Dependency> &dependencies,
    const std::string &field_name)
{
  if (dependencies.size() > kMaxRemotePublishDependenciesPerTable)
  {
    throw spio::PublishError(
        "remote publish dependency table exceeds the " +
        std::to_string(kMaxRemotePublishDependenciesPerTable) + "-entry limit: " + field_name);
  }

  std::vector<spio::Dependency> ordered = dependencies;
  std::sort(
      ordered.begin(),
      ordered.end(),
      [](const spio::Dependency &left, const spio::Dependency &right) {
        return left.alias < right.alias;
      });

  nlohmann::json serialized = nlohmann::json::array();
  for (const spio::Dependency &dependency : ordered)
  {
    if (dependency.source_kind != spio::DependencySourceKind::kRegistry ||
        !dependency.package.has_value() ||
        !dependency.version.has_value())
    {
      throw spio::PublishError("remote publish dependencies must be registry-addressable: " + field_name);
    }
    ValidateBoundedPublishString(dependency.alias, field_name + ".alias", kMaxRemotePublishAliasBytes);
    ValidateBoundedPublishString(
        *dependency.package,
        field_name + ".package",
        kMaxRemotePublishPackageBytes);
    ValidateBoundedPublishString(
        *dependency.version,
        field_name + ".version_req",
        kMaxRemotePublishVersionBytes);
    ValidateBoundedPublishString(
        dependency.source,
        field_name + ".registry",
        kMaxRemotePublishRegistryBytes);
    serialized.push_back({
        {"alias", dependency.alias},
        {"package", *dependency.package},
        {"version_req", *dependency.version},
        {"registry", dependency.source},
    });
  }
  return serialized;
}

}  // namespace

namespace spio
{

PublishResult PreparePublishCandidate(const PublishRequest &request)
{
  const PackageSelection selection = SelectPackageForPublish(request);
  ValidatePublishCandidate(selection);

  const PackResult package_archive = WriteSourcePackage({
      .manifest_path = selection.manifest_path,
      .package_name = selection.manifest.package->name,
      .output_path = request.output_path,
  });

  return {
      .manifest_path = selection.manifest_path,
      .package_root = selection.package_root,
      .archive_path = package_archive.archive_path,
      .package_name = selection.manifest.package->name,
      .package_version = selection.manifest.package->version,
      .dependencies = selection.manifest.package->dependencies,
      .dev_dependencies = selection.manifest.package->dev_dependencies,
      .dependency_count = selection.manifest.package->dependencies.size(),
      .dev_dependency_count = selection.manifest.package->dev_dependencies.size(),
  };
}

RemotePublishArchive ReadRemotePublishArchive(const fs::path &archive_path, const std::uintmax_t max_size_bytes)
{
  if (max_size_bytes == 0U)
  {
    throw PublishError("remote publish archive size limit must be greater than zero");
  }

  std::error_code error;
  if (!fs::is_regular_file(archive_path, error) || error)
  {
    throw PublishError("remote publish archive must be a readable regular file");
  }

  const std::uintmax_t archive_size = fs::file_size(archive_path, error);
  if (error)
  {
    throw PublishError("unable to determine remote publish archive size");
  }
  if (archive_size == 0U)
  {
    throw PublishError("remote publish archive must not be empty");
  }
  if (archive_size > max_size_bytes)
  {
    throw PublishError(
        "remote publish archive exceeds the configured " + std::to_string(max_size_bytes) + "-byte limit");
  }
  if (archive_size > static_cast<std::uintmax_t>(std::numeric_limits<size_t>::max()))
  {
    throw PublishError("remote publish archive is too large for this process");
  }

  std::ifstream input(archive_path, std::ios::binary);
  if (!input)
  {
    throw PublishError("unable to open remote publish archive");
  }

  std::string bytes(static_cast<size_t>(archive_size), '\0');
  input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (input.gcount() != static_cast<std::streamsize>(bytes.size()) || input.bad())
  {
    throw PublishError("unable to read complete remote publish archive");
  }

  return {
      .name = archive_path.filename().string(),
      .base64 = EncodeBase64(bytes),
      .sha256 = Sha256Text(bytes),
      .size_bytes = archive_size,
  };
}

std::string BuildRemotePublishRequestJson(const PublishResult &candidate, const std::uintmax_t max_size_bytes)
{
  const RemotePublishArchive archive = ReadRemotePublishArchive(candidate.archive_path, max_size_bytes);
  ValidateBoundedPublishString(candidate.package_name, "package", kMaxRemotePublishPackageBytes);
  ValidateBoundedPublishString(candidate.package_version, "version", kMaxRemotePublishVersionBytes);
  ValidateBoundedPublishString(archive.name, "archive_name", kMaxRemotePublishArchiveNameBytes);
  return nlohmann::json{
      {"package", candidate.package_name},
      {"version", candidate.package_version},
      {"archive_name", archive.name},
      {"archive_base64", archive.base64},
      {"archive_sha256", archive.sha256},
      {"archive_size_bytes", archive.size_bytes},
      {"publisher_id", "pafio-cli"},
      {"dependencies", SerializeRemotePublishDependencies(candidate.dependencies, "dependencies")},
      {"dev_dependencies", SerializeRemotePublishDependencies(candidate.dev_dependencies, "dev_dependencies")},
  }.dump();
}

}  // namespace spio
