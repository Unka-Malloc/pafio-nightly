#include "SpioCLI/CLI.hpp"
#include "SpioCore/Errors.hpp"
#include "SpioTool/Install.hpp"

#include "ToolTestSupport.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace spio_test_support;

namespace
{

void WriteNoopExecutable(const fs::path &path)
{
  WriteExecutable(
      path,
      "#!/bin/sh\n"
      "exit 0\n");
}

}  // namespace

TEST(DoctorTests, ReportsPlatformReleaseTargetAndManagedStyio)
{
  const fs::path root = MakeTempDir("doctor-json");
  const fs::path fake_bin = root / "bin";
  fs::create_directories(fake_bin);
  for (const std::string &program : {"curl", "install", "shasum", "git", "cmake", "styio"})
  {
    WriteNoopExecutable(fake_bin / program);
  }

  const ScopedEnvVar spio_home("SPIO_HOME", (root / ".spio-home").string());
  const ScopedEnvVar path("PATH", fake_bin.string());
  const ScopedEnvVar libc("SPIO_TOOL_RELEASE_LIBC", "glibc");

  const fs::path manifest_path = root / "project/spio.toml";
  WriteSingleBinManifest(manifest_path);

  const fs::path fake_styio = root / "fake-styio";
  WriteFakeStyio(fake_styio, "0.0.5");
  (void) spio::InstallManagedStyio({.styio_binary = fake_styio});

  testing::internal::CaptureStdout();
  const int exit_code = spio::RunCli({
      "--json",
      "doctor",
      "--manifest-path",
      manifest_path.string(),
      "--release-root",
      "https://packages.styio.dev",
  });
  const std::string stdout_text = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, spio::kExitSuccess);
  const json payload = json::parse(stdout_text);
  EXPECT_EQ(payload.at("command").get<std::string>(), "doctor");
  EXPECT_TRUE(payload.at("ok").get<bool>());
  EXPECT_TRUE(payload.at("platform").at("ok").get<bool>());
  EXPECT_FALSE(payload.at("platform").at("styio_release_target").get<std::string>().empty());
  EXPECT_TRUE(payload.at("release_root").at("configured").get<bool>());
  EXPECT_EQ(payload.at("release_root").at("root").get<std::string>(), "https://packages.styio.dev");
  EXPECT_EQ(payload.at("tool_status").at("current_compiler").at("compiler_version").get<std::string>(), "0.0.5");

  bool saw_current_check = false;
  for (const json &check : payload.at("checks"))
  {
    if (check.at("name").get<std::string>() == "managed_current_styio")
    {
      saw_current_check = true;
      EXPECT_EQ(check.at("status").get<std::string>(), "ok");
    }
  }
  EXPECT_TRUE(saw_current_check);
}
