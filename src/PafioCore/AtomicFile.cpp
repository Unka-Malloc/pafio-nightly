#include "PafioCore/AtomicFile.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <vector>

#include "PafioCore/Errors.hpp"

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{

#if defined(_WIN32)
fs::path
UniqueTemporaryPath(const fs::path &path) {
  static std::atomic<uint64_t> sequence{0};
  const uint64_t nonce = static_cast<uint64_t>(
    std::chrono::steady_clock::now().time_since_epoch().count()
  );
  return path.parent_path() / (path.filename().string() + ".tmp." + std::to_string(nonce) + "." + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
}
#endif

void
RemoveTemporaryFile(const fs::path &path) noexcept {
  std::error_code ignored;
  fs::remove(path, ignored);
}

#if !defined(_WIN32)
void
SynchronizeParentDirectory(const fs::path &path) {
  const fs::path parent = path.parent_path().empty() ? fs::path(".") : path.parent_path();
  const int directory_fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
  if (directory_fd < 0) {
    throw pafio::CacheError(
      "failed to open atomic write parent directory for synchronization: " + std::string(std::strerror(errno))
    );
  }
  if (::fsync(directory_fd) != 0) {
    const int sync_error = errno;
    ::close(directory_fd);
    throw pafio::CacheError(
      "failed to synchronize atomic write parent directory: " + std::string(std::strerror(sync_error))
    );
  }
  if (::close(directory_fd) != 0) {
    throw pafio::CacheError(
      "failed to close atomic write parent directory: " + std::string(std::strerror(errno))
    );
  }
}
#endif

}  // namespace

namespace pafio
{

void
AtomicWriteFile(const fs::path &path, std::string_view content) {
  if (path.empty() || path.filename().empty()) {
    throw CacheError("atomic write target must be a file path");
  }
  if (!path.parent_path().empty()) {
    fs::create_directories(path.parent_path());
  }
#if defined(_WIN32)
  const fs::path temp_path = UniqueTemporaryPath(path);
  try {
    std::ofstream out(temp_path, std::ios::binary);
    if (!out) {
      throw CacheError("failed to open temporary file for atomic write: " + temp_path.string());
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out.good()) {
      throw CacheError("failed to write temporary file for atomic write: " + temp_path.string());
    }
    out.close();
    if (!out) {
      throw CacheError("failed to close temporary file for atomic write: " + temp_path.string());
    }
    std::error_code ec;
    fs::rename(temp_path, path, ec);
    if (ec) {
      throw CacheError("failed to finalize atomic write: " + path.string() + ": " + ec.message());
    }
  }
  catch (...) {
    RemoveTemporaryFile(temp_path);
    throw;
  }
#else
  std::string pattern = (path.parent_path() / (path.filename().string() + ".tmp.XXXXXX")).string();
  std::vector<char> pattern_buffer(pattern.begin(), pattern.end());
  pattern_buffer.push_back('\0');
  int fd = ::mkstemp(pattern_buffer.data());
  if (fd < 0) {
    throw CacheError(
      "failed to create temporary file for atomic write: " + std::string(std::strerror(errno))
    );
  }
  const fs::path temp_path(pattern_buffer.data());
  try {
    size_t offset = 0;
    while (offset < content.size()) {
      const size_t remaining = content.size() - offset;
      const size_t chunk = std::min(
        remaining,
        static_cast<size_t>(std::numeric_limits<ssize_t>::max())
      );
      const ssize_t written = ::write(fd, content.data() + offset, chunk);
      if (written > 0) {
        offset += static_cast<size_t>(written);
        continue;
      }
      if (written < 0 && errno == EINTR) {
        continue;
      }
      throw CacheError(
        "failed to write temporary file for atomic write: " + std::string(std::strerror(errno))
      );
    }
    if (::fsync(fd) != 0) {
      throw CacheError(
        "failed to synchronize temporary file: " + std::string(std::strerror(errno))
      );
    }
    if (::close(fd) != 0) {
      fd = -1;
      throw CacheError(
        "failed to close synchronized temporary file: " + std::string(std::strerror(errno))
      );
    }
    fd = -1;

    std::error_code ec;
    fs::rename(temp_path, path, ec);
    if (ec) {
      throw CacheError(
        "failed to finalize atomic write: " + path.string() + ": " + ec.message()
      );
    }
    SynchronizeParentDirectory(path);
  }
  catch (...) {
    if (fd >= 0) {
      ::close(fd);
    }
    RemoveTemporaryFile(temp_path);
    throw;
  }
#endif
}

}  // namespace pafio
