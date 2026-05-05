#include "SpioTool/PrebuiltInstall.hpp"

#include "SpioCore/Errors.hpp"
#include "SpioCore/Paths.hpp"
#include "SpioCore/Process.hpp"
#include "SpioCore/Sha256.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/utsname.h>

namespace fs = std::filesystem;

namespace
{

constexpr uintmax_t kMaxToolBinaryBytes = 512ULL * 1024ULL * 1024ULL;

bool StartsWith(const std::string &value, const std::string &prefix)
{
  return value.rfind(prefix, 0U) == 0U;
}

bool IsAsciiWhitespace(const unsigned char ch)
{
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

std::string Trim(std::string value)
{
  while (!value.empty() && IsAsciiWhitespace(static_cast<unsigned char>(value.front())))
  {
    value.erase(value.begin());
  }
  while (!value.empty() && IsAsciiWhitespace(static_cast<unsigned char>(value.back())))
  {
    value.pop_back();
  }
  return value;
}

std::string FirstField(const std::string &text)
{
  std::istringstream in(text);
  std::string field;
  in >> field;
  return field;
}

bool IsSafePathSegment(const std::string &value)
{
  if (value.empty() || value == "." || value == "..")
  {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](const unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '.' || ch == '_' || ch == '-';
  });
}

bool IsSha256Hex(const std::string &value)
{
  if (value.size() != 64U)
  {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](const unsigned char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
  });
}

std::string NormalizeReleaseRoot(std::string root)
{
  root = Trim(std::move(root));
  while (root.size() > 1U && root.back() == '/')
  {
    root.pop_back();
  }
  if (root.empty())
  {
    throw spio::ToolError("tool release root is empty");
  }
  if (StartsWith(root, "http://") || StartsWith(root, "https://") || StartsWith(root, "file://"))
  {
    return root;
  }
  if (root.front() == '/')
  {
    return "file://" + root;
  }
  throw spio::ToolError("tool release root must be http://, https://, file://, or an absolute path: " + root);
}

std::optional<std::string> NonEmptyEnv(const char *name)
{
  if (const char *value = std::getenv(name); value != nullptr && value[0] != '\0')
  {
    return std::string(value);
  }
  return std::nullopt;
}

std::string Lowercase(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool LinuxUsesMusl()
{
  if (const std::optional<std::string> libc = NonEmptyEnv("SPIO_TOOL_RELEASE_LIBC"); libc.has_value())
  {
    const std::string normalized = Lowercase(*libc);
    if (normalized == "musl")
    {
      return true;
    }
    if (normalized == "glibc")
    {
      return false;
    }
    throw spio::ToolError("unsupported SPIO_TOOL_RELEASE_LIBC value: " + *libc);
  }

  if (fs::exists("/etc/alpine-release"))
  {
    return true;
  }

  static const std::vector<fs::path> musl_loaders = {
      "/lib/ld-musl-aarch64.so.1",
      "/lib/ld-musl-x86_64.so.1",
      "/lib/ld-musl-armhf.so.1",
      "/lib/ld-musl-armv7.so.1",
  };
  for (const fs::path &loader : musl_loaders)
  {
    if (fs::exists(loader))
    {
      return true;
    }
  }

  return false;
}

std::optional<std::string> ReadConfiguredReleaseRoot()
{
  const fs::path config_path = spio::ResolveSpioHome() / "config" / "tool-release-root";
  std::ifstream in(config_path);
  if (!in)
  {
    return std::nullopt;
  }
  std::string line;
  std::getline(in, line);
  line = Trim(std::move(line));
  if (line.empty())
  {
    return std::nullopt;
  }
  return line;
}

std::string JoinUrl(const std::string &root, const std::string &relative_path)
{
  return root + "/" + relative_path;
}

std::vector<std::string> CurlPolicyArgs()
{
  return {
      "--connect-timeout",
      "10",
      "--max-time",
      "120",
      "--speed-time",
      "10",
      "--speed-limit",
      "1024",
  };
}

std::string FetchTextFirstField(const std::string &url)
{
  std::vector<std::string> args{"-fsSL"};
  const std::vector<std::string> policy_args = CurlPolicyArgs();
  args.insert(args.end(), policy_args.begin(), policy_args.end());
  args.push_back("--max-filesize");
  args.push_back("4096");
  args.push_back(url);

  const spio::ProcessResult result = spio::RunProcess<spio::ToolError>({
      .program = "curl",
      .args = args,
      .timeout = spio::kExternalProcessStepTimeout,
      .max_stdout_bytes = 4096,
      .error_context = "tool release fetch",
  });
  if (result.exit_code != 0)
  {
    throw spio::ToolError("failed to fetch tool release object '" + url + "': " + spio::DescribeProcessFailure(result));
  }
  std::string field = FirstField(result.stdout_text);
  if (field.empty())
  {
    throw spio::ToolError("tool release object is empty: " + url);
  }
  return field;
}

void FetchUrlToFile(const std::string &url, const fs::path &path)
{
  fs::create_directories(path.parent_path());
  const fs::path temp_path = path.parent_path() / (path.filename().string() + ".tmp");
  std::vector<std::string> args{"-fsSL"};
  const std::vector<std::string> policy_args = CurlPolicyArgs();
  args.insert(args.end(), policy_args.begin(), policy_args.end());
  args.push_back("--max-filesize");
  args.push_back(std::to_string(kMaxToolBinaryBytes));
  args.push_back("-o");
  args.push_back(temp_path.string());
  args.push_back(url);

  const spio::ProcessResult result = spio::RunProcess<spio::ToolError>({
      .program = "curl",
      .args = args,
      .timeout = spio::kExternalProcessStepTimeout,
      .error_context = "tool release fetch",
  });
  if (result.exit_code != 0)
  {
    std::error_code ignored;
    fs::remove(temp_path, ignored);
    throw spio::ToolError("failed to fetch tool release object '" + url + "': " + spio::DescribeProcessFailure(result));
  }

  std::error_code size_error;
  const uintmax_t size = fs::file_size(temp_path, size_error);
  if (size_error || size > kMaxToolBinaryBytes)
  {
    std::error_code ignored;
    fs::remove(temp_path, ignored);
    throw spio::ToolError("tool release object exceeded response limit: " + url);
  }

  std::error_code rename_error;
  fs::rename(temp_path, path, rename_error);
  if (rename_error)
  {
    std::error_code ignored;
    fs::remove(path, ignored);
    fs::rename(temp_path, path, rename_error);
  }
  if (rename_error)
  {
    std::error_code ignored;
    fs::remove(temp_path, ignored);
    throw spio::CacheError("failed to finalize downloaded tool release: " + path.string());
  }
}

fs::path ToolReleaseDownloadPath(
    const fs::path &spio_home,
    const std::string &release_target,
    const std::string &platform,
    const std::string &version)
{
  return spio_home / "cache" / "tool-releases" / release_target / platform / version / "styio";
}

void MarkExecutable(const fs::path &path)
{
  std::error_code ec;
  fs::permissions(
      path,
      fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
      fs::perm_options::add,
      ec);
  if (ec)
  {
    throw spio::CacheError("failed to mark downloaded styio executable: " + path.string());
  }
}

}  // namespace

namespace spio
{

std::optional<ResolvedToolReleaseRoot> ResolveStyioToolReleaseRoot(
    const std::optional<std::string> &explicit_release_root)
{
  if (explicit_release_root.has_value() && !explicit_release_root->empty())
  {
    return ResolvedToolReleaseRoot{
        .root = NormalizeReleaseRoot(*explicit_release_root),
        .source = "argument",
        .explicit_root = true,
    };
  }
  if (const std::optional<std::string> root = NonEmptyEnv("SPIO_STYIO_RELEASE_ROOT"); root.has_value())
  {
    return ResolvedToolReleaseRoot{
        .root = NormalizeReleaseRoot(*root),
        .source = "SPIO_STYIO_RELEASE_ROOT",
        .explicit_root = false,
    };
  }
  if (const std::optional<std::string> root = NonEmptyEnv("SPIO_TOOL_RELEASE_ROOT"); root.has_value())
  {
    return ResolvedToolReleaseRoot{
        .root = NormalizeReleaseRoot(*root),
        .source = "SPIO_TOOL_RELEASE_ROOT",
        .explicit_root = false,
    };
  }
  if (const std::optional<std::string> root = ReadConfiguredReleaseRoot(); root.has_value())
  {
    return ResolvedToolReleaseRoot{
        .root = NormalizeReleaseRoot(*root),
        .source = "SPIO_HOME/config/tool-release-root",
        .explicit_root = false,
    };
  }
  return std::nullopt;
}

std::string DetectToolReleasePlatform()
{
  utsname info{};
  if (uname(&info) != 0)
  {
    throw ToolError("failed to detect platform for tool release");
  }
  std::string os = info.sysname;
  std::string arch = info.machine;
  os = Lowercase(os);
  arch = Lowercase(arch);

  if (os == "darwin")
  {
    os = "darwin";
  }
  else if (os == "linux")
  {
    os = LinuxUsesMusl() ? "linux-musl" : "linux";
  }
  else
  {
    throw ToolError("unsupported OS for tool release platform detection: " + std::string(info.sysname));
  }

  if (arch == "arm64" || arch == "aarch64")
  {
    arch = "aarch64";
  }
  else if (arch == "x86_64" || arch == "amd64")
  {
    arch = "x86_64";
  }
  else
  {
    throw ToolError("unsupported CPU for tool release platform detection: " + std::string(info.machine));
  }
  return os + "-" + arch;
}

std::string DetectStyioClientReleaseTarget(const std::string &platform)
{
  if (StartsWith(platform, "linux-") || StartsWith(platform, "linux-musl-"))
  {
    return "styio-linux";
  }
  if (StartsWith(platform, "darwin-"))
  {
    return "styio-macos-cli";
  }
  if (StartsWith(platform, "windows-"))
  {
    return "styio-windows-cli";
  }
  throw ToolError("unsupported styio client release target for platform: " + platform);
}

PrebuiltStyioInstallResult InstallPrebuiltStyio(const PrebuiltStyioInstallRequest &request)
{
  const std::string platform = request.platform.value_or(DetectToolReleasePlatform());
  if (!IsSafePathSegment(platform))
  {
    throw ToolError("invalid tool release platform: " + platform);
  }
  const std::string release_target = request.release_target.value_or(DetectStyioClientReleaseTarget(platform));
  if (!IsSafePathSegment(release_target))
  {
    throw ToolError("invalid styio release target: " + release_target);
  }

  std::string version = request.requested;
  if (version.empty())
  {
    throw ToolError("styio release request is empty");
  }
  const std::string root = NormalizeReleaseRoot(request.release_root.root);
  const std::string release_channel = request.release_channel.empty() ? "stable" : request.release_channel;
  if (!IsSafePathSegment(release_channel))
  {
    throw ToolError("invalid styio release channel: " + release_channel);
  }

  if (version == "latest")
  {
    const std::string version_url =
        JoinUrl(root, "tools/" + release_target + "/channel/" + release_channel + "/" + platform + "/version");
    try
    {
      version = FetchTextFirstField(version_url);
    }
    catch (const ToolError &)
    {
      if (release_target == "styio")
      {
        throw;
      }
      const std::string legacy_version_url =
          JoinUrl(root, "tools/styio/channel/" + release_channel + "/" + platform + "/version");
      version = FetchTextFirstField(legacy_version_url);
    }
  }
  if (!IsSafePathSegment(version))
  {
    throw ToolError("invalid styio release version: " + version);
  }

  std::string resolved_release_target = release_target;
  std::string binary_url = JoinUrl(root, "tools/" + resolved_release_target + "/releases/" + version + "/" + platform + "/styio");
  std::string sha256_url = binary_url + ".sha256";
  std::string expected_sha256;
  try
  {
    expected_sha256 = FetchTextFirstField(sha256_url);
  }
  catch (const ToolError &)
  {
    if (release_target == "styio")
    {
      throw;
    }
    resolved_release_target = "styio";
    binary_url = JoinUrl(root, "tools/styio/releases/" + version + "/" + platform + "/styio");
    sha256_url = binary_url + ".sha256";
    expected_sha256 = FetchTextFirstField(sha256_url);
  }
  if (!IsSha256Hex(expected_sha256))
  {
    throw ToolError("styio release sha256 is invalid for " + sha256_url);
  }

  const fs::path spio_home = ResolveSpioHome();
  const fs::path downloaded_binary_path = ToolReleaseDownloadPath(spio_home, resolved_release_target, platform, version);
  FetchUrlToFile(binary_url, downloaded_binary_path);
  const std::string actual_sha256 = Sha256File(downloaded_binary_path);
  if (actual_sha256 != expected_sha256)
  {
    throw CacheError(
        "styio release sha256 mismatch for " + binary_url + ": expected " + expected_sha256 + ", got " + actual_sha256);
  }
  MarkExecutable(downloaded_binary_path);

  ToolInstallResult install = InstallManagedStyio({.styio_binary = downloaded_binary_path});
  return PrebuiltStyioInstallResult{
      .install = std::move(install),
      .release_root = root,
      .release_root_source = request.release_root.source,
      .release_channel = release_channel,
      .release_version = version,
      .release_target = resolved_release_target,
      .platform = platform,
      .binary_url = binary_url,
      .sha256_url = sha256_url,
      .sha256 = expected_sha256,
      .downloaded_binary_path = downloaded_binary_path,
  };
}

}  // namespace spio
