#include "SpioApp/DoctorApp.hpp"

#include "SpioCLI/Support.hpp"
#include "SpioCloud/Execution.hpp"
#include "SpioCore/Errors.hpp"
#include "SpioCore/Paths.hpp"
#include "SpioManifest/Manifest.hpp"
#include "SpioTool/Contract.hpp"
#include "SpioTool/Install.hpp"
#include "SpioTool/PrebuiltInstall.hpp"
#include "SpioToolchain/State.hpp"
#include "SpioToolchain/Vocabulary.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace spio
{

namespace
{

struct DoctorCheck
{
  std::string name;
  std::string status;
  std::string message;
  json detail = json::object();
};

bool IsExecutable(const fs::path &path)
{
#if defined(_WIN32)
  return _access(path.string().c_str(), 0) == 0;
#else
  return access(path.string().c_str(), X_OK) == 0;
#endif
}

std::optional<fs::path> FindProgramOnPath(const std::string &program)
{
  const char *path_value = std::getenv("PATH");
  if (path_value == nullptr || path_value[0] == '\0')
  {
    return std::nullopt;
  }

#if defined(_WIN32)
  constexpr char kPathSeparator = ';';
#else
  constexpr char kPathSeparator = ':';
#endif

  std::stringstream stream(path_value);
  std::string entry;
  while (std::getline(stream, entry, kPathSeparator))
  {
    if (entry.empty())
    {
      continue;
    }
    const fs::path candidate = fs::path(entry) / program;
    if (IsExecutable(candidate))
    {
      return candidate;
    }
  }
  return std::nullopt;
}

json SerializeCheck(const DoctorCheck &check)
{
  json payload = {
      {"name", check.name},
      {"status", check.status},
      {"message", check.message},
  };
  if (!check.detail.empty())
  {
    payload["detail"] = check.detail;
  }
  return payload;
}

bool HasError(const std::vector<DoctorCheck> &checks)
{
  for (const DoctorCheck &check : checks)
  {
    if (check.status == "error")
    {
      return true;
    }
  }
  return false;
}

void AddProgramCheck(
    std::vector<DoctorCheck> &checks,
    std::string name,
    std::string program,
    std::string ok_message,
    std::string missing_message,
    std::string missing_status)
{
  if (const std::optional<fs::path> path = FindProgramOnPath(program); path.has_value())
  {
    checks.push_back({
        .name = std::move(name),
        .status = "ok",
        .message = std::move(ok_message),
        .detail = {{"program", program}, {"path", path->string()}},
    });
    return;
  }
  checks.push_back({
      .name = std::move(name),
      .status = std::move(missing_status),
      .message = std::move(missing_message),
      .detail = {{"program", program}},
  });
}

void PrintHumanDoctor(const json &payload)
{
  std::cout << "spio doctor\n";
  std::cout << "ok: " << (payload.at("ok").get<bool>() ? "true" : "false") << '\n';
  std::cout << "SPIO_HOME: " << payload.at("spio_home").get<std::string>() << '\n';

  const json &platform = payload.at("platform");
  if (platform.at("ok").get<bool>())
  {
    std::cout << "platform: " << platform.at("value").get<std::string>() << '\n';
    std::cout << "styio release target: " << platform.at("styio_release_target").get<std::string>() << '\n';
  }
  else
  {
    std::cout << "platform: error: " << platform.at("error").get<std::string>() << '\n';
  }

  const json &release_root = payload.at("release_root");
  if (release_root.at("configured").get<bool>())
  {
    std::cout << "release root: " << release_root.at("root").get<std::string>()
              << " (" << release_root.at("source").get<std::string>() << ")\n";
  }
  else
  {
    std::cout << "release root: not configured\n";
  }

  std::cout << "checks:\n";
  for (const json &check : payload.at("checks"))
  {
    std::cout << "  [" << check.at("status").get<std::string>() << "] "
              << check.at("name").get<std::string>() << ": "
              << check.at("message").get<std::string>() << '\n';
  }
}

}  // namespace

int HandleDoctor(const std::vector<std::string> &args, bool as_json)
{
  if (args.size() == 1 && args.front() == "--help")
  {
    return PrintCommandUsage("doctor");
  }

  bool local_json = as_json;
  std::optional<fs::path> manifest_path;
  std::optional<std::string> release_root_arg;
  std::string release_channel = std::string(kChannelStable);

  for (size_t index = 0; index < args.size(); ++index)
  {
    if (args[index] == "--json")
    {
      local_json = true;
    }
    else if (args[index] == "--manifest-path")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--manifest-path requires a value", "doctor"}, as_json);
      }
      manifest_path = fs::path(args[index]);
    }
    else if (args[index] == "--release-root")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--release-root requires a value", "doctor"}, as_json);
      }
      release_root_arg = args[index];
    }
    else if (args[index] == "--channel")
    {
      if (++index >= args.size())
      {
        return EmitError({"UsageError", kExitUsage, "--channel requires a value", "doctor"}, as_json);
      }
      release_channel = NormalizeSetKeyword(args[index]);
      if (!IsSupportedChannel(release_channel))
      {
        return EmitError({"UsageError", kExitUsage, "--channel must be stable or nightly", "doctor"}, as_json);
      }
    }
    else
    {
      return EmitError({"UsageError", kExitUsage, "unexpected argument for doctor: " + args[index], "doctor"}, as_json);
    }
  }

  std::vector<DoctorCheck> checks;
  json platform_payload = {{"ok", false}};
  std::optional<std::string> platform;
  std::optional<std::string> release_target;

  try
  {
    platform = DetectToolReleasePlatform();
    release_target = DetectStyioClientReleaseTarget(*platform);
    platform_payload = {
        {"ok", true},
        {"value", *platform},
        {"styio_release_target", *release_target},
    };
    checks.push_back({
        .name = "release_platform",
        .status = "ok",
        .message = "detected supported tool release platform " + *platform,
    });
  }
  catch (const ToolError &err)
  {
    platform_payload = {
        {"ok", false},
        {"error", err.what()},
    };
    checks.push_back({
        .name = "release_platform",
        .status = "error",
        .message = err.what(),
    });
  }

  AddProgramCheck(
      checks,
      "curl",
      "curl",
      "curl is available for prebuilt tool downloads",
      "curl is missing; prebuilt spio/styio downloads will fail",
      "error");
  AddProgramCheck(
      checks,
      "install_command",
      "install",
      "install is available for install-spio.sh",
      "install is missing; the shell installer may need --install-dir plus a local copy fallback",
      "warning");
  if (FindProgramOnPath("sha256sum").has_value() || FindProgramOnPath("shasum").has_value())
  {
    checks.push_back({
        .name = "installer_checksum_tool",
        .status = "ok",
        .message = "sha256sum or shasum is available for install-spio.sh checksum verification",
    });
  }
  else
  {
    checks.push_back({
        .name = "installer_checksum_tool",
        .status = "warning",
        .message = "sha256sum or shasum is missing; install-spio.sh cannot verify downloaded spio checksums",
    });
  }
  AddProgramCheck(
      checks,
      "git",
      "git",
      "git is available for source-build fallback",
      "git is missing; source-build fallback cannot fetch the Styio source tree",
      "warning");
  AddProgramCheck(
      checks,
      "cmake",
      "cmake",
      "cmake is available for source-build fallback",
      "cmake is missing; source-build fallback cannot configure the Styio compiler build",
      "warning");

  json release_root_payload = {
      {"configured", false},
      {"channel", release_channel},
  };
  try
  {
    const std::optional<ResolvedToolReleaseRoot> resolved = ResolveStyioToolReleaseRoot(release_root_arg);
    if (resolved.has_value())
    {
      release_root_payload = {
          {"configured", true},
          {"root", resolved->root},
          {"source", resolved->source},
          {"explicit", resolved->explicit_root},
          {"channel", release_channel},
      };
      if (platform.has_value() && release_target.has_value())
      {
        release_root_payload["styio_channel_version_url"] =
            resolved->root + "/tools/" + *release_target + "/channel/" + release_channel + "/" + *platform + "/version";
        release_root_payload["legacy_styio_channel_version_url"] =
            resolved->root + "/tools/styio/channel/" + release_channel + "/" + *platform + "/version";
      }
      checks.push_back({
          .name = "release_root",
          .status = "ok",
          .message = "tool release root is configured from " + resolved->source,
      });
    }
    else
    {
      checks.push_back({
          .name = "release_root",
          .status = "warning",
          .message = "tool release root is not configured; spio install styio@latest will use source-build unless --release-root is provided",
      });
    }
  }
  catch (const ToolError &err)
  {
    release_root_payload = {
        {"configured", false},
        {"channel", release_channel},
        {"error", err.what()},
    };
    checks.push_back({
        .name = "release_root",
        .status = "error",
        .message = err.what(),
    });
  }
  catch (const CacheError &err)
  {
    release_root_payload = {
        {"configured", false},
        {"channel", release_channel},
        {"error", err.what()},
    };
    checks.push_back({
        .name = "release_root",
        .status = "error",
        .message = err.what(),
    });
  }

  json tool_status_payload = nullptr;
  try
  {
    std::optional<ProjectToolchainState> project_state;
    std::optional<CloudExecutionPolicy> cloud_policy;
    if (manifest_path.has_value())
    {
      (void) LoadManifest(*manifest_path);
      project_state = LoadProjectToolchainState(*manifest_path);
      cloud_policy = ResolveCloudExecutionPolicy(*project_state);
      checks.push_back({
          .name = "manifest",
          .status = "ok",
          .message = "manifest is valid: " + CanonicalAbsolutePath(*manifest_path).string(),
      });
    }
    const ToolStatusResult status = QueryToolStatus(manifest_path);
    tool_status_payload = BuildToolStatusPayload(status, project_state, cloud_policy);
    if (status.current_compiler.has_value())
    {
      checks.push_back({
          .name = "managed_current_styio",
          .status = "ok",
          .message = "managed current styio is installed: " + status.current_compiler->install_binary_path.string(),
          .detail = {
              {"compiler_version", status.current_compiler->compiler_version},
              {"channel", status.current_compiler->compiler_channel},
          },
      });
    }
    else
    {
      checks.push_back({
          .name = "managed_current_styio",
          .status = "warning",
          .message = "managed current styio is not installed; run spio install styio@latest",
      });
    }
  }
  catch (const ValidationError &err)
  {
    checks.push_back({
        .name = "manifest",
        .status = "error",
        .message = err.what(),
    });
  }
  catch (const ToolError &err)
  {
    checks.push_back({
        .name = "managed_toolchain",
        .status = "error",
        .message = err.what(),
    });
  }
  catch (const CacheError &err)
  {
    checks.push_back({
        .name = "spio_home",
        .status = "error",
        .message = err.what(),
    });
  }

  if (const std::optional<fs::path> styio_path = FindProgramOnPath("styio"); styio_path.has_value())
  {
    checks.push_back({
        .name = "styio_path_shim",
        .status = "ok",
        .message = "styio command is available on PATH",
        .detail = {{"path", styio_path->string()}},
    });
  }
  else
  {
    checks.push_back({
        .name = "styio_path_shim",
        .status = "warning",
        .message = "styio command is not on PATH; install-spio.sh normally installs a shim beside spio",
    });
  }

  fs::path spio_home;
  try
  {
    spio_home = ResolveSpioHome();
  }
  catch (const CacheError &)
  {
    spio_home = fs::path{};
  }

  json payload = {
      {"command", "doctor"},
      {"ok", !HasError(checks)},
      {"spio_home", spio_home.empty() ? "" : spio_home.string()},
      {"platform", platform_payload},
      {"release_root", release_root_payload},
      {"checks", json::array()},
      {"tool_status", tool_status_payload},
  };
  for (const DoctorCheck &check : checks)
  {
    payload["checks"].push_back(SerializeCheck(check));
  }

  if (local_json)
  {
    std::cout << payload.dump() << '\n';
  }
  else
  {
    PrintHumanDoctor(payload);
  }
  return payload.at("ok").get<bool>() ? kExitSuccess : kExitToolInstall;
}

}  // namespace spio
