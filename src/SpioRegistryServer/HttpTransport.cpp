#include "SpioRegistryServer/HttpTransport.hpp"

#include "SpioCore/Errors.hpp"
#include "SpioCore/Process.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

namespace
{

fs::path MakeTempPath(std::string_view suffix)
{
  static uint64_t counter = 0;
  ++counter;
  return fs::temp_directory_path() /
         ("spio-http-registry-" + std::to_string(static_cast<long long>(getpid())) + "-" + std::to_string(counter) +
          std::string(suffix));
}

}  // namespace

namespace spio
{

RegistryHttpResponse CurlCommandRegistryHttpTransport::Perform(const RegistryHttpRequest &request) const
{
  const fs::path body_path = MakeTempPath(".body");
  std::vector<std::string> args{
      "-sS",
      "-L",
      "-o",
      body_path.string(),
      "-w",
      "%{http_code}",
  };
  if (request.method == "HEAD")
  {
    args.push_back("-I");
  }
  else
  {
    args.push_back("-X");
    args.push_back(request.method);
  }
  if (request.content_type.has_value())
  {
    args.push_back("-H");
    args.push_back("Content-Type: " + *request.content_type);
  }
  for (const std::string &header : request.request_headers)
  {
    args.push_back("-H");
    args.push_back(header);
  }
  if (request.upload_path.has_value())
  {
    args.push_back("--upload-file");
    args.push_back(request.upload_path->string());
  }
  args.push_back(request.url);

  const spio::ProcessResult result = spio::RunProcess<spio::PublishError>({
      .program = "curl",
      .args = args,
      .error_context = "remote registry publish",
  });
  std::string body;
  if (fs::exists(body_path))
  {
    std::ifstream in(body_path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    body = buffer.str();
    fs::remove(body_path);
  }

  if (result.exit_code != 0)
  {
    throw PublishError("curl failed for remote registry request: " + request.method + " " + request.url + ": " +
                       spio::TrimTrailingNewline(result.stderr_text));
  }

  const std::string status_text = spio::TrimTrailingNewline(result.stdout_text);
  int status_code = 0;
  try
  {
    status_code = std::stoi(status_text);
  }
  catch (const std::exception &)
  {
    throw PublishError("curl did not report a valid HTTP status for remote registry request: " + request.method + " " +
                       request.url);
  }

  return {
      .status_code = status_code,
      .body = std::move(body),
  };
}

const RegistryHttpTransport &DefaultRegistryHttpTransport()
{
  static const CurlCommandRegistryHttpTransport kTransport;
  return kTransport;
}

}  // namespace spio
