#pragma once

#include <filesystem>
#include <string_view>

namespace spio
{

// Same-directory temp + fsync + rename for mutable state files.
void AtomicWriteFile(const std::filesystem::path &path, std::string_view content);

}  // namespace spio
