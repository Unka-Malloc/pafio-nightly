#include "SpioApp/PackageApp.hpp"

#include "SpioCLI/Support.hpp"
#include "SpioManifest/Lockfile.hpp"
#include "SpioManifest/Manifest.hpp"
#include "SpioPack/Pack.hpp"
#include "SpioPublish/Publish.hpp"
#include "SpioRegistryServer/Publish.hpp"
#include "SpioResolve/Resolver.hpp"
#include "SpioSecurity/RegistrySecurity.hpp"
#include "SpioTree/Render.hpp"
#include "SpioVendor/Vendor.hpp"
#include "SpioWorkflow/Dependencies.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace spio
{

int HandleNew(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("new");
  }
  if (args.empty())
  {
    return EmitError({"UsageError", kExitUsage, "new requires a package name", "new"}, as_json);
  }

  size_t index = 0;
  const std::string name = args[index++];
  fs::path directory;
  if (index < args.size() && !args[index].starts_with("--"))
  {
    directory = fs::path(args[index++]);
  }
  else
  {
    const auto slash = name.find('/');
    directory = slash == std::string::npos ? fs::path(name) : fs::path(name.substr(slash + 1));
  }

  const std::string kind = KindFromFlags(args, index);
  if (index != args.size())
  {
    return EmitError({"UsageError", kExitUsage, "unexpected arguments for new", "new"}, as_json);
  }

  try
  {
    InitializeProject({.package_name = name, .root = directory, .kind = kind});
  }
  catch (const std::exception &err)
  {
    return EmitError({"UsageError", kExitUsage, err.what(), "new"}, as_json);
  }

  return EmitSuccess(
      {
          {"command", "new"},
          {"message", "initialized project at " + directory.string()},
          {"root", fs::absolute(directory).string()},
      },
      as_json);
}

int HandleInit(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("init");
  }
  std::optional<std::string> name;
  size_t index = 0;
  while (index < args.size())
  {
    if (args[index] == "--name")
    {
      ++index;
      if (index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--name requires a value", "init"}, as_json);
      }
      name = args[index++];
      continue;
    }
    break;
  }

  const std::string kind = KindFromFlags(args, index);
  if (index != args.size())
  {
    return EmitError({"UsageError", kExitUsage, "unexpected arguments for init", "init"}, as_json);
  }

  const fs::path root = fs::current_path();
  const std::string package_name = name.value_or(InferLocalPackageName(root));
  try
  {
    InitializeProject({.package_name = package_name, .root = root, .kind = kind});
  }
  catch (const std::exception &err)
  {
    return EmitError({"UsageError", kExitUsage, err.what(), "init"}, as_json);
  }

  return EmitSuccess(
      {
          {"command", "init"},
          {"message", "initialized project in " + root.string()},
          {"root", fs::absolute(root).string()},
          {"package", package_name},
      },
      as_json);
}

int HandleAdd(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("add");
  }
  if (args.empty())
  {
    return EmitError({"UsageError", kExitUsage, "add requires a package name", "add"}, as_json);
  }

  AddDependencyRequest request{
      .manifest_path = "spio.toml",
      .package_name = args.front(),
  };
  bool saw_source = false;
  for (size_t index = 1; index < args.size(); ++index)
  {
    if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "add"}, as_json);
      }
      request.manifest_path = args[index];
    }
    else if (args[index] == "--alias")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--alias requires a value", "add"}, as_json);
      }
      request.alias = args[index];
    }
    else if (args[index] == "--dev")
    {
      request.section = DependencySection::kDevDependencies;
    }
    else if (args[index] == "--path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--path requires a value", "add"}, as_json);
      }
      if (saw_source)
      {
        return EmitError({"UsageError", kExitUsage, "add accepts exactly one dependency source", "add"}, as_json);
      }
      request.use_git = false;
      request.source = args[index];
      saw_source = true;
    }
    else if (args[index] == "--git")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--git requires a value", "add"}, as_json);
      }
      if (saw_source)
      {
        return EmitError({"UsageError", kExitUsage, "add accepts exactly one dependency source", "add"}, as_json);
      }
      request.use_git = true;
      request.source = args[index];
      saw_source = true;
    }
    else if (args[index] == "--registry")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--registry requires a value", "add"}, as_json);
      }
      if (saw_source)
      {
        return EmitError({"UsageError", kExitUsage, "add accepts exactly one dependency source", "add"}, as_json);
      }
      request.use_registry = true;
      request.source = args[index];
      saw_source = true;
    }
    else if (args[index] == "--rev")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--rev requires a value", "add"}, as_json);
      }
      request.rev = args[index];
    }
    else if (args[index] == "--version")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--version requires a value", "add"}, as_json);
      }
      request.version = args[index];
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for add: " + args[index], "add"}, as_json);
    }
  }

  if (!saw_source)
  {
    return EmitError(
        {"UsageError", kExitUsage, "add requires --path <path>, --git <source> --rev <rev>, or --registry <url> --version <x.y.z>", "add"},
        as_json);
  }

  try
  {
    const DependencyCommandResult result = AddDependencyAndRefreshLock(request);
    return EmitSuccess(
        {
            {"command", "add"},
            {"message", "added dependency '" + result.alias + "' and refreshed lockfile"},
            {"manifest_path", result.manifest_path.string()},
            {"lockfile_path", result.lockfile_path.string()},
            {"alias", result.alias},
            {"package", result.package_name},
            {"section", DependencySectionName(result.section)},
            {"packages", result.package_count},
            {"source_kind", request.use_git ? "git" : (request.use_registry ? "registry" : "path")},
        },
        as_json);
  }
  catch (const ValidationError &err)
  {
    return EmitError({"ManifestError", kExitManifest, err.what(), "add"}, as_json);
  }
  catch (const ResolutionError &err)
  {
    return EmitError({"ResolutionError", kExitResolve, err.what(), "add"}, as_json);
  }
  catch (const FetchError &err)
  {
    return EmitError({"FetchError", kExitFetch, err.what(), "add"}, as_json);
  }
  catch (const CacheError &err)
  {
    return EmitError({"CacheError", kExitCache, err.what(), "add"}, as_json);
  }
}

int HandleRemove(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("remove");
  }
  if (args.empty())
  {
    return EmitError({"UsageError", kExitUsage, "remove requires a dependency alias or package name", "remove"}, as_json);
  }

  RemoveDependencyRequest request{
      .manifest_path = "spio.toml",
      .target = args.front(),
  };
  for (size_t index = 1; index < args.size(); ++index)
  {
    if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "remove"}, as_json);
      }
      request.manifest_path = args[index];
    }
    else if (args[index] == "--dev")
    {
      request.section = DependencySection::kDevDependencies;
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for remove: " + args[index], "remove"}, as_json);
    }
  }

  try
  {
    const DependencyCommandResult result = RemoveDependencyAndRefreshLock(request);
    return EmitSuccess(
        {
            {"command", "remove"},
            {"message", "removed dependency '" + result.alias + "' and refreshed lockfile"},
            {"manifest_path", result.manifest_path.string()},
            {"lockfile_path", result.lockfile_path.string()},
            {"alias", result.alias},
            {"package", result.package_name},
            {"section", DependencySectionName(result.section)},
            {"packages", result.package_count},
        },
        as_json);
  }
  catch (const ValidationError &err)
  {
    return EmitError({"ManifestError", kExitManifest, err.what(), "remove"}, as_json);
  }
  catch (const ResolutionError &err)
  {
    return EmitError({"ResolutionError", kExitResolve, err.what(), "remove"}, as_json);
  }
  catch (const FetchError &err)
  {
    return EmitError({"FetchError", kExitFetch, err.what(), "remove"}, as_json);
  }
  catch (const CacheError &err)
  {
    return EmitError({"CacheError", kExitCache, err.what(), "remove"}, as_json);
  }
}

int HandleFetch(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("fetch");
  }

  fs::path manifest_path = "spio.toml";
  WorkflowFlags workflow_flags;
  for (size_t index = 0; index < args.size(); ++index)
  {
    if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "fetch"}, as_json);
      }
      manifest_path = args[index];
    }
    else if (ConsumeWorkflowFlag(args[index], workflow_flags))
    {
      continue;
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for fetch: " + args[index], "fetch"}, as_json);
    }
  }

  const ResolveOptions resolve_options = BuildResolveOptions(manifest_path, workflow_flags);
  if (const auto lock_policy_error = ValidateLockedPolicy(manifest_path, "fetch", workflow_flags, resolve_options);
      lock_policy_error.has_value())
  {
    return EmitError(*lock_policy_error, as_json);
  }

  try
  {
    const FetchCommandResult result = FetchDependencies(manifest_path, resolve_options);
    return EmitSuccess(
        {
            {"command", "fetch"},
            {"message", "fetched dependency sources for " + std::to_string(result.package_count) + " package(s)"},
            {"manifest_path", result.manifest_path.string()},
            {"packages", result.package_count},
            {"git_packages", result.git_package_count},
            {"registry_packages", result.registry_package_count},
            {"locked", workflow_flags.locked},
            {"offline", workflow_flags.offline},
        },
        as_json);
  }
  catch (const ValidationError &err)
  {
    return EmitError({"ManifestError", kExitManifest, err.what(), "fetch"}, as_json);
  }
  catch (const WorkspaceError &err)
  {
    return EmitError({"WorkspaceError", kExitWorkspace, err.what(), "fetch"}, as_json);
  }
  catch (const ResolutionError &err)
  {
    return EmitError({"ResolutionError", kExitResolve, err.what(), "fetch"}, as_json);
  }
  catch (const FetchError &err)
  {
    return EmitError({"FetchError", kExitFetch, err.what(), "fetch"}, as_json);
  }
  catch (const CacheError &err)
  {
    return EmitError({"CacheError", kExitCache, err.what(), "fetch"}, as_json);
  }
}

int HandleLock(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("lock");
  }

  fs::path manifest_path = "spio.toml";
  bool check_only = false;
  WorkflowFlags workflow_flags;
  for (size_t index = 0; index < args.size(); ++index)
  {
    if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "lock"}, as_json);
      }
      manifest_path = args[index];
    }
    else if (args[index] == "--check")
    {
      check_only = true;
    }
    else if (args[index] == "--offline")
    {
      workflow_flags.offline = true;
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for lock: " + args[index], "lock"}, as_json);
    }
  }

  if (!fs::exists(manifest_path))
  {
    return EmitError({"ManifestError", kExitManifest, "manifest not found: " + manifest_path.string(), "lock"}, as_json);
  }

  LockGenerationResult generated;
  const ResolveOptions resolve_options = BuildResolveOptions(manifest_path, workflow_flags);
  try
  {
    generated = ResolveSingleVersionLockfile(manifest_path, resolve_options);
  }
  catch (const ValidationError &err)
  {
    return EmitError({"ManifestError", kExitManifest, err.what(), "lock"}, as_json);
  }
  catch (const WorkspaceError &err)
  {
    return EmitError({"WorkspaceError", kExitWorkspace, err.what(), "lock"}, as_json);
  }
  catch (const ResolutionError &err)
  {
    return EmitError({"ResolutionError", kExitResolve, err.what(), "lock"}, as_json);
  }
  catch (const FetchError &err)
  {
    return EmitError({"FetchError", kExitFetch, err.what(), "lock"}, as_json);
  }
  catch (const CacheError &err)
  {
    return EmitError({"CacheError", kExitCache, err.what(), "lock"}, as_json);
  }

  const std::string rendered = SerializeLockfileCanonical(generated.lockfile);
  if (check_only)
  {
    if (!fs::exists(generated.lockfile_path))
    {
      return EmitError({"LockfileError", kExitLock, "lockfile missing: " + generated.lockfile_path.string(), "lock"}, as_json);
    }

    try
    {
      if (ReadFile(generated.lockfile_path) != rendered)
      {
        return EmitError({"LockfileError", kExitLock, "lockfile is stale: " + generated.lockfile_path.string(), "lock"}, as_json);
      }
    }
    catch (const std::exception &err)
    {
      return EmitError({"LockfileError", kExitLock, err.what(), "lock"}, as_json);
    }

    return EmitSuccess(
        {
            {"command", "lock"},
            {"message", "lockfile is up to date: " + generated.lockfile_path.string()},
            {"manifest_path", generated.manifest_path.string()},
            {"lockfile_path", generated.lockfile_path.string()},
            {"mode", "check"},
            {"packages", generated.lockfile.packages.size()},
            {"offline", workflow_flags.offline},
        },
        as_json);
  }

  try
  {
    std::ofstream out(generated.lockfile_path);
    if (!out)
    {
      throw std::runtime_error("failed to open lockfile for write: " + generated.lockfile_path.string());
    }
    out << rendered;
    if (!out.good())
    {
      throw std::runtime_error("failed to write lockfile: " + generated.lockfile_path.string());
    }
  }
  catch (const std::exception &err)
  {
    return EmitError({"LockfileError", kExitLock, err.what(), "lock"}, as_json);
  }

  return EmitSuccess(
      {
          {"command", "lock"},
          {"message", "wrote lockfile: " + generated.lockfile_path.string()},
          {"manifest_path", generated.manifest_path.string()},
          {"lockfile_path", generated.lockfile_path.string()},
          {"mode", "write"},
          {"packages", generated.lockfile.packages.size()},
          {"offline", workflow_flags.offline},
      },
      as_json);
}

int HandleTree(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("tree");
  }

  fs::path manifest_path = "spio.toml";
  for (size_t index = 0; index < args.size(); ++index)
  {
    if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "tree"}, as_json);
      }
      manifest_path = args[index];
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for tree: " + args[index], "tree"}, as_json);
    }
  }

  if (!fs::exists(manifest_path))
  {
    return EmitError({"ManifestError", kExitManifest, "manifest not found: " + manifest_path.string(), "tree"}, as_json);
  }

  LockGenerationResult graph;
  try
  {
    graph = ResolveSingleVersionLockfile(manifest_path);
  }
  catch (const ValidationError &err)
  {
    return EmitError({"ManifestError", kExitManifest, err.what(), "tree"}, as_json);
  }
  catch (const WorkspaceError &err)
  {
    return EmitError({"WorkspaceError", kExitWorkspace, err.what(), "tree"}, as_json);
  }
  catch (const ResolutionError &err)
  {
    return EmitError({"ResolutionError", kExitResolve, err.what(), "tree"}, as_json);
  }
  catch (const FetchError &err)
  {
    return EmitError({"FetchError", kExitFetch, err.what(), "tree"}, as_json);
  }
  catch (const CacheError &err)
  {
    return EmitError({"CacheError", kExitCache, err.what(), "tree"}, as_json);
  }

  if (as_json)
  {
    json packages = json::array();
    for (const LockPackage &package : graph.lockfile.packages)
    {
      json item{
          {"id", package.id},
          {"name", package.name},
          {"version", package.version},
          {"source_kind", package.source_kind},
          {"dependencies", package.dependencies},
      };
      if (package.git.has_value())
      {
        item["git"] = *package.git;
      }
      if (package.rev.has_value())
      {
        item["rev"] = *package.rev;
      }
      if (package.registry.has_value())
      {
        item["registry"] = *package.registry;
      }
      if (package.sha256.has_value())
      {
        item["sha256"] = *package.sha256;
      }
      packages.push_back(std::move(item));
    }

    std::cout << json({
                     {"command", "tree"},
                     {"manifest_path", graph.manifest_path.string()},
                     {"lockfile_path", graph.lockfile_path.string()},
                     {"resolver", graph.lockfile.resolver},
                     {"root_ids", graph.root_ids},
                     {"packages", packages},
                 })
                     .dump()
              << '\n';
    return kExitSuccess;
  }

  std::cout << RenderDependencyTreeText(graph);
  return kExitSuccess;
}

int HandleVendor(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("vendor");
  }

  VendorRequest request;
  WorkflowFlags workflow_flags;
  for (size_t index = 0; index < args.size(); ++index)
  {
    if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "vendor"}, as_json);
      }
      request.manifest_path = args[index];
    }
    else if (args[index] == "--output")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--output requires a value", "vendor"}, as_json);
      }
      request.output_path = fs::path(args[index]);
    }
    else if (ConsumeWorkflowFlag(args[index], workflow_flags))
    {
      continue;
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for vendor: " + args[index], "vendor"}, as_json);
    }
  }

  const ResolveOptions resolve_options = BuildResolveOptions(request.manifest_path, workflow_flags, request.output_path);
  if (const auto lock_policy_error = ValidateLockedPolicy(request.manifest_path, "vendor", workflow_flags, resolve_options);
      lock_policy_error.has_value())
  {
    return EmitError(*lock_policy_error, as_json);
  }
  request.offline = workflow_flags.offline;

  try
  {
    const VendorResult result = WriteVendorTree(request);
    return EmitSuccess(
        {
            {"command", "vendor"},
            {"message", "materialized vendored dependency snapshots under " + result.vendor_root.string()},
            {"manifest_path", result.manifest_path.string()},
            {"vendor_root", result.vendor_root.string()},
            {"metadata_path", result.metadata_path.string()},
            {"packages", result.package_count},
            {"git_snapshots", result.git_snapshot_count},
            {"locked", workflow_flags.locked},
            {"offline", workflow_flags.offline},
        },
        as_json);
  }
  catch (const ValidationError &err)
  {
    return EmitError({"ManifestError", kExitManifest, err.what(), "vendor"}, as_json);
  }
  catch (const WorkspaceError &err)
  {
    return EmitError({"WorkspaceError", kExitWorkspace, err.what(), "vendor"}, as_json);
  }
  catch (const ResolutionError &err)
  {
    return EmitError({"ResolutionError", kExitResolve, err.what(), "vendor"}, as_json);
  }
  catch (const FetchError &err)
  {
    return EmitError({"FetchError", kExitFetch, err.what(), "vendor"}, as_json);
  }
  catch (const CacheError &err)
  {
    return EmitError({"CacheError", kExitCache, err.what(), "vendor"}, as_json);
  }
  catch (const VendorError &err)
  {
    return EmitError({"VendorError", kExitVendor, err.what(), "vendor"}, as_json);
  }
}

int HandlePack(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("pack");
  }

  PackRequest request;
  for (size_t index = 0; index < args.size(); ++index)
  {
    if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "pack"}, as_json);
      }
      request.manifest_path = args[index];
    }
    else if (args[index] == "--package")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--package requires a value", "pack"}, as_json);
      }
      request.package_name = args[index];
    }
    else if (args[index] == "--output")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--output requires a value", "pack"}, as_json);
      }
      request.output_path = args[index];
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for pack: " + args[index], "pack"}, as_json);
    }
  }

  try
  {
    const PackResult result = WriteSourcePackage(request);
    return EmitSuccess(
        {
            {"command", "pack"},
            {"message", "wrote source package: " + result.archive_path.string()},
            {"manifest_path", result.manifest_path.string()},
            {"package_root", result.package_root.string()},
            {"archive_path", result.archive_path.string()},
            {"archive_prefix", result.archive_prefix},
            {"package", result.package_name},
            {"version", result.package_version},
            {"files", result.file_count},
        },
        as_json);
  }
  catch (const ValidationError &err)
  {
    return EmitError({"ManifestError", kExitManifest, err.what(), "pack"}, as_json);
  }
  catch (const WorkspaceError &err)
  {
    return EmitError({"WorkspaceError", kExitWorkspace, err.what(), "pack"}, as_json);
  }
  catch (const PackError &err)
  {
    return EmitError({"PackError", kExitPack, err.what(), "pack"}, as_json);
  }
}

int HandlePublish(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("publish");
  }

  PublishRequest request;
  std::optional<std::string> registry_root;
  std::optional<std::string> registry_profile;
  std::optional<fs::path> registry_policy_file;
  std::vector<std::string> registry_headers;
  bool dry_run = false;
  for (size_t index = 0; index < args.size(); ++index)
  {
    if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "publish"}, as_json);
      }
      request.manifest_path = args[index];
    }
    else if (args[index] == "--package")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--package requires a value", "publish"}, as_json);
      }
      request.package_name = args[index];
    }
    else if (args[index] == "--output")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--output requires a value", "publish"}, as_json);
      }
      request.output_path = args[index];
    }
    else if (args[index] == "--registry")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--registry requires a value", "publish"}, as_json);
      }
      registry_root = args[index];
    }
    else if (args[index] == "--registry-profile")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--registry-profile requires a value", "publish"}, as_json);
      }
      registry_profile = args[index];
    }
    else if (args[index] == "--registry-policy-file")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--registry-policy-file requires a value", "publish"}, as_json);
      }
      registry_policy_file = args[index];
    }
    else if (args[index] == "--registry-header")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--registry-header requires a value", "publish"}, as_json);
      }
      const std::optional<std::string> normalized = NormalizeRegistryHeader(args[index]);
      if (!normalized.has_value())
      {
        return EmitError(
            {"UsageError", kExitUsage, "--registry-header must match <name:value> with non-empty name and value", "publish"},
            as_json);
      }
      registry_headers.push_back(*normalized);
    }
    else if (args[index] == "--dry-run")
    {
      dry_run = true;
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for publish: " + args[index], "publish"}, as_json);
    }
  }

  if (dry_run && !registry_headers.empty())
  {
    return EmitError(
        {"UsageError", kExitUsage, "--registry-header is not valid with --dry-run", "publish"},
        as_json);
  }
  if (dry_run && registry_policy_file.has_value())
  {
    return EmitError(
        {"UsageError", kExitUsage, "--registry-policy-file is not valid with --dry-run", "publish"},
        as_json);
  }
  if (dry_run && registry_profile.has_value())
  {
    return EmitError(
        {"UsageError", kExitUsage, "--registry-profile is not valid with --dry-run", "publish"},
        as_json);
  }
  if (registry_profile.has_value() && registry_policy_file.has_value())
  {
    return EmitError(
        {"UsageError", kExitUsage, "--registry-profile cannot be combined with --registry-policy-file", "publish"},
        as_json);
  }

  if (!dry_run)
  {
    if (!registry_root.has_value())
    {
      return EmitError(
          {"UsageError", kExitUsage, "publish requires --registry <path-or-url> unless --dry-run is set", "publish"},
          as_json);
    }

    try
    {
      if (IsHttpRegistryRoot(*registry_root))
      {
        const RegistryWriteSecurityDecision security = ResolveRegistryWriteSecurity({
            .registry_root = *registry_root,
            .profile_name = registry_profile,
            .policy_file = registry_policy_file,
            .explicit_request_headers = registry_headers,
        });
        const HttpRegistryPublishResult result = PublishToHttpRegistry({
            .publish_request = request,
            .registry_root = security.registry_root,
            .request_headers = security.request_headers,
        });
        json payload = {
            {"command", "publish"},
            {"mode", "publish"},
            {"transport", "http"},
            {"message", "published package into remote registry: " + result.registry_entry_url},
            {"manifest_path", result.candidate.manifest_path.string()},
            {"package_root", result.candidate.package_root.string()},
            {"archive_path", result.candidate.archive_path.string()},
            {"package", result.candidate.package_name},
            {"version", result.candidate.package_version},
            {"dependencies", result.candidate.dependency_count},
            {"dev_dependencies", result.candidate.dev_dependency_count},
            {"registry_root", result.registry_root},
            {"registry_marker_url", result.registry_marker_url},
            {"registry_blob_url", result.registry_blob_url},
            {"registry_entry_url", result.registry_entry_url},
            {"registry_security_provider", security.provider_name},
            {"registry_write_security_mode", security.mode},
            {"registry_header_count", security.request_headers.size()},
            {"sha256", result.archive_sha256},
            {"size_bytes", result.archive_size_bytes},
            {"published_at", result.published_at_utc},
        };
        if (security.profile_name.has_value())
        {
          payload["registry_profile"] = *security.profile_name;
        }
        return EmitSuccess(payload, as_json);
      }

      const fs::path filesystem_registry_root =
          IsFileRegistryRoot(*registry_root) ? FileRegistryUrlToPath(*registry_root) : fs::path(*registry_root);
      if (registry_policy_file.has_value())
      {
        return EmitError(
            {
                "UsageError",
                kExitUsage,
                "--registry-policy-file is only valid for http:// or https:// registry roots",
                "publish",
            },
            as_json);
      }
      if (registry_profile.has_value())
      {
        return EmitError(
            {
                "UsageError",
                kExitUsage,
                "--registry-profile is only valid for http:// or https:// registry roots",
                "publish",
            },
            as_json);
      }
      if (!registry_headers.empty())
      {
        return EmitError(
            {"UsageError", kExitUsage, "--registry-header is only valid for http:// or https:// registry roots", "publish"},
            as_json);
      }
      const RegistryPublishResult result = PublishToFilesystemRegistry({
          .publish_request = request,
          .registry_root = filesystem_registry_root,
      });
      return EmitSuccess(
          {
              {"command", "publish"},
              {"mode", "publish"},
              {"transport", "filesystem"},
              {"message", "published package into local filesystem registry: " + result.registry_entry_path.string()},
              {"manifest_path", result.candidate.manifest_path.string()},
              {"package_root", result.candidate.package_root.string()},
              {"archive_path", result.candidate.archive_path.string()},
              {"package", result.candidate.package_name},
              {"version", result.candidate.package_version},
              {"dependencies", result.candidate.dependency_count},
              {"dev_dependencies", result.candidate.dev_dependency_count},
              {"registry_root", result.registry_root.string()},
              {"registry_marker_path", result.registry_marker_path.string()},
              {"registry_blob_path", result.registry_blob_path.string()},
              {"registry_entry_path", result.registry_entry_path.string()},
              {"sha256", result.archive_sha256},
              {"size_bytes", result.archive_size_bytes},
              {"published_at", result.published_at_utc},
          },
          as_json);
    }
    catch (const ValidationError &err)
    {
      return EmitError({"ManifestError", kExitManifest, err.what(), "publish"}, as_json);
    }
    catch (const WorkspaceError &err)
    {
      return EmitError({"WorkspaceError", kExitWorkspace, err.what(), "publish"}, as_json);
    }
    catch (const PackError &err)
    {
      return EmitError({"PackError", kExitPack, err.what(), "publish"}, as_json);
    }
    catch (const PublishError &err)
    {
      return EmitError({"PublishError", kExitPublish, err.what(), "publish"}, as_json);
    }
  }

  try
  {
    const PublishResult result = PreparePublishCandidate(request);
    return EmitSuccess(
        {
            {"command", "publish"},
            {"mode", "dry-run"},
            {"message", "prepared publish candidate: " + result.archive_path.string()},
            {"manifest_path", result.manifest_path.string()},
            {"package_root", result.package_root.string()},
            {"archive_path", result.archive_path.string()},
            {"package", result.package_name},
            {"version", result.package_version},
            {"dependencies", result.dependency_count},
            {"dev_dependencies", result.dev_dependency_count},
        },
        as_json);
  }
  catch (const ValidationError &err)
  {
    return EmitError({"ManifestError", kExitManifest, err.what(), "publish"}, as_json);
  }
  catch (const WorkspaceError &err)
  {
    return EmitError({"WorkspaceError", kExitWorkspace, err.what(), "publish"}, as_json);
  }
  catch (const PackError &err)
  {
    return EmitError({"PackError", kExitPack, err.what(), "publish"}, as_json);
  }
  catch (const PublishError &err)
  {
    return EmitError({"PublishError", kExitPublish, err.what(), "publish"}, as_json);
  }
}

}  // namespace spio
