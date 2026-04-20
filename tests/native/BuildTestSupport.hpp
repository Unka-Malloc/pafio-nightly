#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace spio::testsupport
{

class ScopedEnvVar
{
public:
  ScopedEnvVar(const std::string &name, const std::string &value)
      : name_(name)
  {
    if (const char *existing = std::getenv(name.c_str()); existing != nullptr)
    {
      had_previous_ = true;
      previous_value_ = existing;
    }
    setenv(name.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvVar()
  {
    if (had_previous_)
    {
      setenv(name_.c_str(), previous_value_.c_str(), 1);
    }
    else
    {
      unsetenv(name_.c_str());
    }
  }

private:
  std::string name_;
  bool had_previous_ = false;
  std::string previous_value_;
};

inline fs::path CanonicalAbsolutePath(const fs::path &path)
{
  return fs::absolute(path).lexically_normal();
}

inline fs::path MakeTempDir(const std::string &label)
{
  const fs::path root = fs::temp_directory_path() / "spio-native-build-tests" / label;
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

inline void WriteFile(const fs::path &path, const std::string &content)
{
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  ASSERT_TRUE(out.good());
  out << content;
  ASSERT_TRUE(out.good());
}

inline std::string ReadFile(const fs::path &path)
{
  std::ifstream in(path);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

inline void WriteExecutable(const fs::path &path, const std::string &content)
{
  WriteFile(path, content);
  fs::permissions(
      path,
      fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
      fs::perm_options::add);
}

inline void WriteFakeSourceToolchain(const fs::path &root)
{
  WriteFile(
      root / "CMakeLists.txt",
      "cmake_minimum_required(VERSION 3.20)\n"
      "project(fake_styio LANGUAGES NONE)\n"
      "file(MAKE_DIRECTORY \"${CMAKE_BINARY_DIR}/bin\")\n"
      "configure_file(\"${CMAKE_SOURCE_DIR}/styio.sh.in\" \"${CMAKE_BINARY_DIR}/bin/styio\" @ONLY NEWLINE_STYLE UNIX)\n"
      "file(CHMOD \"${CMAKE_BINARY_DIR}/bin/styio\" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)\n"
      "add_custom_target(styio ALL DEPENDS \"${CMAKE_BINARY_DIR}/bin/styio\")\n");
  WriteFile(
      root / "styio.sh.in",
      "#!/bin/sh\n"
      "if [ \"$1\" = \"--compile-plan\" ]; then\n"
      "  python3 - \"$2\" <<'PY'\n"
      "import json, os, sys\n"
      "plan = json.load(open(sys.argv[1], 'r', encoding='utf-8'))\n"
      "for key in ('build_root', 'artifact_dir', 'diag_dir'):\n"
      "    os.makedirs(plan['outputs'][key], exist_ok=True)\n"
      "print('fake source toolchain executed compile-plan')\n"
      "PY\n"
      "  exit 0\n"
      "fi\n"
      "echo unexpected invocation >&2\n"
      "exit 64\n");
}

}  // namespace spio::testsupport
