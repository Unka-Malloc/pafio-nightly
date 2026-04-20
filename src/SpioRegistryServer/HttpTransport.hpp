#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace spio
{

struct RegistryHttpRequest
{
  std::string method;
  std::string url;
  std::vector<std::string> request_headers;
  std::optional<std::filesystem::path> upload_path;
  std::optional<std::string> content_type;
};

struct RegistryHttpResponse
{
  int status_code = 0;
  std::string body;
};

class RegistryHttpTransport
{
public:
  virtual ~RegistryHttpTransport() = default;
  virtual RegistryHttpResponse Perform(const RegistryHttpRequest &request) const = 0;
};

class CurlCommandRegistryHttpTransport final : public RegistryHttpTransport
{
public:
  RegistryHttpResponse Perform(const RegistryHttpRequest &request) const override;
};

const RegistryHttpTransport &DefaultRegistryHttpTransport();

}  // namespace spio
