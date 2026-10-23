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

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{

fs::path
UniqueTemporaryPath(const fs::path &path) {
  static std::atomic<uint64_t> sequence{0};
  const uint64_t nonce = static_cast<uint64_t>(
    std::chrono::steady_clock::now().time_since_epoch().count()
  );
  return path.parent_path() / (path.filename().string() + ".tmp." + std::to_string(nonce) + "." + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
}

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

// Collisions only mean the nonce landed on an existing name, so retry with a
// fresh suffix; the bound matches mkstemp's "give up after implausible
// contention" semantics rather than an attacker-driven infinite loop.
ExclusiveTempFile
OpenExclusiveTemporaryFile(const fs::path &target) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    const fs::path candidate = UniqueTemporaryPath(target);
#if defined(_WIN32)
    const HANDLE handle = CreateFileW(
      candidate.c_str(),
      GENERIC_WRITE,
      0,
      nullptr,
      CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL,
      nullptr
    );
    if (handle != INVALID_HANDLE_VALUE) {
      return ExclusiveTempFile{candidate, handle};
    }
    if (GetLastError() != ERROR_FILE_EXISTS) {
      throw std::system_error(
        static_cast<int>(GetLastError()),
        std::system_category(),
        "create exclusive temporary file " + candidate.string()
      );
    }
#else
    std::string pattern_string = (candidate).string();
    std::vector<char> pattern(pattern_string.begin(), pattern_string.end());
    pattern.push_back('\0');
    const int fd = ::mkstemp(pattern.data());
    if (fd >= 0) {
      return ExclusiveTempFile{fs::path(pattern.data()), fd};
    }
    if (errno != EEXIST) {
      throw std::system_error(
        errno,
        std::system_category(),
        "create exclusive temporary file " + candidate.string()
      );
    }
#endif
  }
  throw std::system_error(std::make_error_code(std::errc::file_exists));
}

void
WriteTemporaryFileFully(ExclusiveTempFile &temp, const char *data, size_t size) {
  size_t offset = 0;
  while (offset < size) {
#if defined(_WIN32)
    DWORD written = 0;
    const DWORD chunk =
        static_cast<DWORD>(std::min(size - offset, static_cast<size_t>(std::numeric_limits<DWORD>::max())));
    if (WriteFile(temp.handle, data + offset, chunk, &written, nullptr) == 0) {
      throw std::system_error(
        static_cast<int>(GetLastError()),
        std::system_category(),
        "write temporary file " + temp.path.string()
      );
    }
    if (written == 0) {
      throw std::system_error(
        std::make_error_code(std::errc::io_error), "write temporary file " + temp.path.string()
      );
    }
    offset += written;
#else
    const ssize_t written = ::write(temp.fd, data + offset, size - offset);
    if (written > 0) {
      offset += static_cast<size_t>(written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    throw std::system_error(errno, std::system_category(), "write temporary file " + temp.path.string());
#endif
  }
}

void
FlushTemporaryFile(ExclusiveTempFile &temp) {
#if defined(_WIN32)
  if (FlushFileBuffers(temp.handle) == 0) {
    throw std::system_error(
      static_cast<int>(GetLastError()),
      std::system_category(),
      "flush temporary file " + temp.path.string()
    );
  }
#else
  if (::fsync(temp.fd) != 0) {
    throw std::system_error(errno, std::system_category(), "flush temporary file " + temp.path.string());
  }
#endif
}

void
CloseTemporaryFile(ExclusiveTempFile &temp) noexcept {
#if defined(_WIN32)
  if (temp.handle != nullptr) {
    CloseHandle(temp.handle);
    temp.handle = nullptr;
  }
#else
  if (temp.fd >= 0) {
    ::close(temp.fd);
    temp.fd = -1;
  }
#endif
}

void
AtomicWriteFile(const fs::path &path, std::string_view content) {
  if (path.empty() || path.filename().empty()) {
    throw CacheError("atomic write target must be a file path");
  }
  if (!path.parent_path().empty()) {
    fs::create_directories(path.parent_path());
  }
#if defined(_WIN32)
  ExclusiveTempFile temp;
  try {
    temp = OpenExclusiveTemporaryFile(path);
  }
  catch (const std::system_error &error) {
    throw CacheError(
      "failed to create temporary file for atomic write: " + std::string(error.what())
    );
  }
  const fs::path temp_path = temp.path;
  try {
    WriteTemporaryFileFully(temp, content.data(), content.size());
    FlushTemporaryFile(temp);
    CloseTemporaryFile(temp);
    std::error_code ec;
    fs::rename(temp_path, path, ec);
    if (ec) {
      throw CacheError("failed to finalize atomic write: " + path.string() + ": " + ec.message());
    }
  }
  catch (const std::system_error &error) {
    CloseTemporaryFile(temp);
    RemoveTemporaryFile(temp_path);
    throw CacheError("failed to write temporary file for atomic write: " + std::string(error.what()));
  }
  catch (...) {
    CloseTemporaryFile(temp);
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
