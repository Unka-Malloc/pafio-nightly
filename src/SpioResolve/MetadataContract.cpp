#include "SpioResolve/MetadataContract.hpp"

#include "SpioCore/Paths.hpp"
#include "SpioManifest/Manifest.hpp"

#include <algorithm>
#include <filesystem>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{

json PackageSummary(const spio::ResolvedPackage &package)
{
  return {
      {"id", package.id},
      {"manifest_path", package.manifest_path.string()},
      {"name", package.package.name},
      {"publish", package.package.publish},
      {"root", package.root_dir.string()},
      {"source_kind", package.source_kind},
      {"version", package.package.version},
      {"edition", package.package.edition},
  };
}

}  // namespace

namespace spio
{

MetadataDocument BuildMetadataDocument(
    const fs::path &manifest_path,
    const ResolvedGraphResult &graph)
{
  const fs::path canonical_manifest = CanonicalAbsolutePath(manifest_path);
  const ManifestDocument manifest = LoadManifest(canonical_manifest);

  json package = json::object();
  if (manifest.package.has_value())
  {
    const auto found = std::find_if(
        graph.packages.begin(),
        graph.packages.end(),
        [&](const ResolvedPackage &candidate) {
          return candidate.manifest_path == canonical_manifest;
        });
    if (found != graph.packages.end())
    {
      package = PackageSummary(*found);
    }
  }

  json workspace = {
      {"exclude", json::array()},
      {"manifest_path", canonical_manifest.string()},
      {"members", json::array()},
      {"resolver", "1"},
      {"root", canonical_manifest.parent_path().string()},
      {"root_package_ids", graph.root_ids},
  };
  if (manifest.workspace.has_value())
  {
    workspace["members"] = manifest.workspace->members;
    workspace["exclude"] = manifest.workspace->exclude;
    workspace["resolver"] = manifest.workspace->resolver;
  }

  std::vector<const ResolvedPackage *> ordered_packages;
  ordered_packages.reserve(graph.packages.size());
  for (const ResolvedPackage &resolved_package : graph.packages)
  {
    ordered_packages.push_back(&resolved_package);
  }
  std::sort(
      ordered_packages.begin(),
      ordered_packages.end(),
      [](const ResolvedPackage *left, const ResolvedPackage *right) {
        return left->id < right->id;
      });

  json dependencies = json::array();
  json targets = json::array();
  for (const ResolvedPackage *resolved_package : ordered_packages)
  {
    std::vector<ResolvedDependencyAlias> aliases =
        resolved_package->dependency_aliases;
    std::sort(
        aliases.begin(),
        aliases.end(),
        [](const ResolvedDependencyAlias &left,
           const ResolvedDependencyAlias &right) {
          return std::tie(left.alias, left.package_id) <
                 std::tie(right.alias, right.package_id);
        });
    for (const ResolvedDependencyAlias &alias : aliases)
    {
      dependencies.push_back({
          {"alias", alias.alias},
          {"package_id", alias.package_id},
          {"parent_package_id", resolved_package->id},
      });
    }

    if (resolved_package->package.lib.has_value())
    {
      targets.push_back({
          {"kind", "lib"},
          {"name", resolved_package->package.name},
          {"package_id", resolved_package->id},
          {"path", CanonicalAbsolutePath(
                       resolved_package->root_dir /
                       resolved_package->package.lib->path)
                       .string()},
      });
    }
    std::vector<BinTarget> bins = resolved_package->package.bins;
    std::sort(
        bins.begin(),
        bins.end(),
        [](const BinTarget &left, const BinTarget &right) {
          return std::tie(left.name, left.path) <
                 std::tie(right.name, right.path);
        });
    for (const BinTarget &bin : bins)
    {
      targets.push_back({
          {"kind", "bin"},
          {"name", bin.name},
          {"package_id", resolved_package->id},
          {"path", CanonicalAbsolutePath(resolved_package->root_dir / bin.path)
                       .string()},
      });
    }
    std::vector<TestTarget> tests = resolved_package->package.tests;
    std::sort(
        tests.begin(),
        tests.end(),
        [](const TestTarget &left, const TestTarget &right) {
          return std::tie(left.name, left.path) <
                 std::tie(right.name, right.path);
        });
    for (const TestTarget &test : tests)
    {
      targets.push_back({
          {"kind", "test"},
          {"name", test.name},
          {"package_id", resolved_package->id},
          {"path", CanonicalAbsolutePath(
                       resolved_package->root_dir / test.path)
                       .string()},
      });
    }
  }

  const fs::path resolution_path =
      ProjectStateRootForManifest(canonical_manifest) / "resolution-v1.json";
  const fs::path vendor_root =
      ProjectVendorRootForManifest(canonical_manifest);
  const fs::path vendor_metadata_path = vendor_root / "spio-vendor.json";

  MetadataDocument document;
  document.package = std::move(package);
  document.workspace = std::move(workspace);
  document.dependencies = std::move(dependencies);
  document.targets = std::move(targets);
  document.lock = {
      {"package_count", graph.lockfile.packages.size()},
      {"path", graph.lockfile_path.string()},
      {"present", fs::is_regular_file(graph.lockfile_path)},
      {"resolver", graph.lockfile.resolver},
  };
  document.resolution = {
      {"package_count", graph.packages.size()},
      {"path", resolution_path.string()},
      {"present", fs::is_regular_file(resolution_path)},
      {"root_package_ids", graph.root_ids},
      {"schema_version", 1},
  };
  document.vendor = {
      {"metadata_path", vendor_metadata_path.string()},
      {"present", fs::is_regular_file(vendor_metadata_path)},
      {"root", vendor_root.string()},
  };
  return document;
}

nlohmann::json SerializeMetadataV1(const MetadataDocument &document)
{
  return {
      {"package", document.package},
      {"workspace", document.workspace},
      {"dependencies", document.dependencies},
      {"targets", document.targets},
      {"lock", document.lock},
      {"resolution", document.resolution},
      {"vendor", document.vendor},
  };
}

}  // namespace spio
