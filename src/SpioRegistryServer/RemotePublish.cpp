#include "SpioRegistryServer/Publish.hpp"

#include "SpioCore/Errors.hpp"
#include "SpioCore/Sha256.hpp"

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{

std::string CurrentUtcTimestamp()
{
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
  gmtime_r(&now, &utc);
  std::ostringstream out;
  out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::string NormalizeRegistryRoot(std::string value)
{
  while (!value.empty() && value.back() == '/')
  {
    value.pop_back();
  }
  return value;
}

bool IsHttpRegistryRoot(const std::string &value)
{
  return value.starts_with("http://") || value.starts_with("https://");
}

std::pair<std::string, std::string> SplitPackageName(const std::string &package_name)
{
  const size_t slash = package_name.find('/');
  if (slash == std::string::npos || slash == 0U || slash + 1U >= package_name.size())
  {
    throw spio::PublishError("package name must match namespace/name for registry publication: " + package_name);
  }
  return {
      package_name.substr(0U, slash),
      package_name.substr(slash + 1U),
  };
}

json ParseJsonObject(const std::string &text, const std::string &context)
{
  try
  {
    json payload = json::parse(text);
    if (!payload.is_object())
    {
      throw spio::PublishError(context + " must be a JSON object");
    }
    return payload;
  }
  catch (const json::parse_error &)
  {
    throw spio::PublishError(context + " is not valid JSON");
  }
}

void ValidateMarker(const json &marker, const std::string &context)
{
  if (marker.value("kind", "") != "filesystem-local" || marker.value("schema_version", 0) != 1)
  {
    throw spio::PublishError(context + " does not match the supported registry marker contract");
  }
}

fs::path MakeTempPath(std::string_view suffix)
{
  static uint64_t counter = 0;
  ++counter;
  return fs::temp_directory_path() / ("spio-http-registry-" + std::to_string(counter) + std::string(suffix));
}

void WriteFileAtomically(const fs::path &path, std::string_view content, const std::string &context)
{
  const fs::path temp_path = MakeTempPath(".tmp");
  {
    std::ofstream out(temp_path, std::ios::binary);
    if (!out)
    {
      throw spio::PublishError("failed to open temporary " + context + " for write: " + temp_path.string());
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out.good())
    {
      throw spio::PublishError("failed to write temporary " + context + ": " + temp_path.string());
    }
  }
  std::error_code ec;
  fs::rename(temp_path, path, ec);
  if (ec)
  {
    fs::remove(temp_path);
    throw spio::PublishError("failed to finalize temporary " + context + ": " + path.string());
  }
}

spio::RegistryHttpResponse PerformHttpRequest(
    const spio::RegistryHttpTransport &transport,
    const std::string &method,
    const std::string &url,
    const std::vector<std::string> &request_headers,
    const std::optional<fs::path> &upload_path = std::nullopt,
    const std::optional<std::string> &content_type = std::nullopt)
{
  return transport.Perform({
      .method = method,
      .url = url,
      .request_headers = request_headers,
      .upload_path = upload_path,
      .content_type = content_type,
  });
}

std::string BuildUrl(const std::string &registry_root, const std::string &relative_path)
{
  return registry_root + "/" + relative_path;
}

std::string BlobRelativePath(const std::string &digest)
{
  return "blobs/sha256/" + digest.substr(0U, 2U) + "/" + digest.substr(2U, 2U) + "/" + digest + ".tar";
}

std::string VersionEntryRelativePath(const std::string &package_name, const std::string &package_version)
{
  const auto [package_namespace, short_name] = SplitPackageName(package_name);
  return "index/" + package_namespace + "/" + short_name + "/" + package_version + ".json";
}

json BuildDependencyArray(const std::vector<spio::Dependency> &dependencies)
{
  json payload = json::array();
  for (const spio::Dependency &dependency : dependencies)
  {
    payload.push_back({
        {"alias", dependency.alias},
        {"package", dependency.package.value_or("")},
        {"version", dependency.version.value_or("")},
        {"registry", dependency.source},
    });
  }
  return payload;
}

json BuildVersionEntry(
    const spio::PublishResult &candidate,
    const std::string &blob_relative_path,
    const std::string &archive_sha256,
    const uint64_t archive_size_bytes,
    const std::string &published_at_utc)
{
  return {
      {"schema_version", 1},
      {"package", candidate.package_name},
      {"version", candidate.package_version},
      {"sha256", archive_sha256},
      {"size_bytes", archive_size_bytes},
      {"blob_path", blob_relative_path},
      {"published_at", published_at_utc},
      {"dependencies", BuildDependencyArray(candidate.dependencies)},
      {"dev_dependencies", BuildDependencyArray(candidate.dev_dependencies)},
  };
}

void EnsureRemoteRegistryMarker(
    const spio::RegistryHttpTransport &transport,
    const std::string &registry_root,
    const std::vector<std::string> &request_headers)
{
  const std::string marker_url = BuildUrl(registry_root, "spio-registry.json");
  const spio::RegistryHttpResponse response = PerformHttpRequest(transport, "GET", marker_url, request_headers);
  if (response.status_code == 200)
  {
    ValidateMarker(ParseJsonObject(response.body, "remote registry marker"), "remote registry marker");
    return;
  }
  if (response.status_code != 404)
  {
    throw spio::PublishError("failed to load remote registry marker: " + marker_url + " returned HTTP " +
                             std::to_string(response.status_code));
  }

  const fs::path marker_file = MakeTempPath(".json");
  WriteFileAtomically(
      marker_file,
      json({
               {"kind", "filesystem-local"},
               {"schema_version", 1},
           })
          .dump(2) +
          "\n",
      "remote registry marker");

  const spio::RegistryHttpResponse put_response =
      PerformHttpRequest(transport, "PUT", marker_url, request_headers, marker_file, "application/json");
  fs::remove(marker_file);
  if (put_response.status_code == 409)
  {
    const spio::RegistryHttpResponse retry_response = PerformHttpRequest(transport, "GET", marker_url, request_headers);
    if (retry_response.status_code == 200)
    {
      ValidateMarker(ParseJsonObject(retry_response.body, "remote registry marker"), "remote registry marker");
      return;
    }
  }

  if (put_response.status_code != 200 && put_response.status_code != 201 && put_response.status_code != 204)
  {
    throw spio::PublishError("failed to create remote registry marker: " + marker_url + " returned HTTP " +
                             std::to_string(put_response.status_code));
  }
}

bool RemoteObjectExists(
    const spio::RegistryHttpTransport &transport,
    const std::string &url,
    const std::vector<std::string> &request_headers)
{
  const spio::RegistryHttpResponse response = PerformHttpRequest(transport, "HEAD", url, request_headers);
  if (response.status_code == 200)
  {
    return true;
  }
  if (response.status_code == 404)
  {
    return false;
  }
  throw spio::PublishError("failed to probe remote registry object: " + url + " returned HTTP " +
                           std::to_string(response.status_code));
}

void UploadRemoteFile(
    const spio::RegistryHttpTransport &transport,
    const std::string &url,
    const std::vector<std::string> &request_headers,
    const fs::path &source_path,
    const std::string &content_type,
    const std::string &context,
    const std::optional<std::string> &duplicate_message = std::nullopt,
    const bool allow_conflict = false)
{
  const spio::RegistryHttpResponse response =
      PerformHttpRequest(transport, "PUT", url, request_headers, source_path, content_type);
  if (response.status_code == 409 && allow_conflict)
  {
    return;
  }
  if (response.status_code == 409 && duplicate_message.has_value())
  {
    throw spio::PublishError(*duplicate_message);
  }
  if (response.status_code != 200 && response.status_code != 201 && response.status_code != 204)
  {
    throw spio::PublishError("failed to upload " + context + ": " + url + " returned HTTP " +
                             std::to_string(response.status_code));
  }
}

}  // namespace

namespace spio
{

HttpRegistryPublishResult PublishToHttpRegistry(
    const HttpRegistryPublishRequest &request,
    const RegistryHttpTransport &transport)
{
  const std::string registry_root = NormalizeRegistryRoot(request.registry_root);
  if (!IsHttpRegistryRoot(registry_root))
  {
    throw PublishError("remote registry publish requires an http:// or https:// registry root: " + request.registry_root);
  }

  const PublishResult candidate = PreparePublishCandidate(request.publish_request);
  EnsureRemoteRegistryMarker(transport, registry_root, request.request_headers);

  const std::string archive_sha256 = spio::Sha256File(candidate.archive_path);
  const std::string blob_relative_path = BlobRelativePath(archive_sha256);
  const std::string entry_relative_path = VersionEntryRelativePath(candidate.package_name, candidate.package_version);
  const std::string blob_url = BuildUrl(registry_root, blob_relative_path);
  const std::string entry_url = BuildUrl(registry_root, entry_relative_path);
  const std::string published_at_utc = CurrentUtcTimestamp();

  std::error_code ec;
  const uint64_t archive_size_bytes = static_cast<uint64_t>(fs::file_size(candidate.archive_path, ec));
  if (ec)
  {
    throw PublishError("failed to inspect archive size for remote registry publish: " + candidate.archive_path.string());
  }

  if (RemoteObjectExists(transport, entry_url, request.request_headers))
  {
    throw PublishError("package version is already published in the target remote registry: " + candidate.package_name + "@" +
                       candidate.package_version);
  }

  if (!RemoteObjectExists(transport, blob_url, request.request_headers))
  {
    UploadRemoteFile(
        transport,
        blob_url,
        request.request_headers,
        candidate.archive_path,
        "application/x-tar",
        "registry blob",
        std::nullopt,
        true);
  }

  const fs::path entry_file = MakeTempPath(".json");
  WriteFileAtomically(
      entry_file,
      BuildVersionEntry(candidate, blob_relative_path, archive_sha256, archive_size_bytes, published_at_utc).dump(2) + "\n",
      "remote registry entry");
  UploadRemoteFile(
      transport,
      entry_url,
      request.request_headers,
      entry_file,
      "application/json",
      "registry entry",
      "package version is already published in the target remote registry: " + candidate.package_name + "@" +
          candidate.package_version);
  fs::remove(entry_file);

  return {
      .candidate = candidate,
      .registry_root = registry_root,
      .registry_marker_url = BuildUrl(registry_root, "spio-registry.json"),
      .registry_blob_url = blob_url,
      .registry_entry_url = entry_url,
      .archive_sha256 = archive_sha256,
      .archive_size_bytes = archive_size_bytes,
      .published_at_utc = published_at_utc,
  };
}

HttpRegistryPublishResult PublishToHttpRegistry(const HttpRegistryPublishRequest &request)
{
  return PublishToHttpRegistry(request, DefaultRegistryHttpTransport());
}

}  // namespace spio
