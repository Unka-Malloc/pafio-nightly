#include "PafioSecurity/Ed25519.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <stdlib.h>
#endif
#if defined(__APPLE__)
#include <unistd.h>
#endif

#include "PafioCore/Errors.hpp"
#include "PafioCore/Process.hpp"
#include "PafioCore/ToolPaths.hpp"

namespace fs = std::filesystem;

namespace
{

bool
DecodeBase64(std::string_view input, std::string &output) {
  static constexpr unsigned char kDecode[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
  };
  // Ed25519 signatures are exactly 64 bytes and therefore have one canonical
  // padded base64 representation of 88 bytes.
  if (input.size() != 88U || !input.ends_with("==")) {
    return false;
  }

  output.clear();
  output.reserve(64U);
  uint32_t val = 0;
  int valb = -8;
  for (size_t index = 0; index < input.size() - 2U; ++index) {
    const unsigned char c = static_cast<unsigned char>(input[index]);
    const unsigned char decoded = kDecode[c];
    if (decoded == 64) {
      return false;
    }
    val = (val << 6U) | decoded;
    valb += 6;
    if (valb >= 0) {
      output.push_back(static_cast<char>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  const unsigned char final_value =
    kDecode[static_cast<unsigned char>(input[input.size() - 3U])];
  return output.size() == 64U && (final_value & 0x0fU) == 0U;
}

class ScopedTemporaryDirectory
{
public:
  ScopedTemporaryDirectory() {
#if defined(_WIN32)
    throw pafio::FetchError("ed25519 verification is not supported on Windows");
#else
    std::string pattern = (fs::temp_directory_path() / "pafio-ed25519-XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    const char *created = ::mkdtemp(buffer.data());
    if (created == nullptr) {
      throw pafio::FetchError("failed to create secure temporary ed25519 verify directory");
    }
    path_ = created;
#endif
  }

  ScopedTemporaryDirectory(const ScopedTemporaryDirectory &) = delete;
  ScopedTemporaryDirectory &operator=(const ScopedTemporaryDirectory &) = delete;

  ~ScopedTemporaryDirectory() {
    std::error_code ignored;
    fs::remove_all(path_, ignored);
  }

  const fs::path &path() const {
    return path_;
  }

private:
  fs::path path_;
};

void
WriteBinaryFile(const fs::path &path, std::string_view content) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw pafio::FetchError("failed to open temporary ed25519 verify file: " + path.string());
  }
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!out.good()) {
    throw pafio::FetchError("failed to write temporary ed25519 verify file: " + path.string());
  }
}

}  // namespace

namespace pafio
{

bool
Ed25519VerifyPem(std::string_view public_key_pem, std::string_view message, std::string_view signature_b64) {
  std::string signature;
  if (!DecodeBase64(signature_b64, signature) || signature.empty()) {
    return false;
  }

  const ScopedTemporaryDirectory temporary_directory;
  const fs::path &temp_root = temporary_directory.path();

  const fs::path message_path = temp_root / "message.bin";
  const fs::path signature_path = temp_root / "message.sig";
  const fs::path public_path = temp_root / "role.pub";
  try {
    WriteBinaryFile(message_path, message);
    WriteBinaryFile(signature_path, signature);
    WriteBinaryFile(public_path, public_key_pem);

    const ProcessResult result = RunProcessChecked({
      .program = ResolvedOpenSslPath(),
      .args =
        {
          "pkeyutl",
          "-verify",
          "-pubin",
          "-inkey",
          public_path.string(),
          "-rawin",
          "-in",
          message_path.string(),
          "-sigfile",
          signature_path.string(),
        },
      .search_path = false,
      .timeout = std::chrono::seconds{30},
      .max_stdout_bytes = 4096,
      .max_stderr_bytes = 4096,
      .error_context = "ed25519 verify",
    });
    return result.exit_code == 0 && !result.timed_out;
  }
  catch (const ProcessFailure &) {
    return false;
  }
}

}  // namespace pafio
