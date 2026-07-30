#include "BuildTestSupport.hpp"

#include "PafioCLI/CLI.hpp"
#include "PafioCore/Errors.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

using pafio::testsupport::MakeTempDir;
using pafio::testsupport::ReadFile;
using pafio::testsupport::ScopedEnvVar;
using pafio::testsupport::WriteExecutable;
using pafio::testsupport::WriteFakeCompilePlanStyio;
using pafio::testsupport::WriteFile;

TEST(BuildCliTests, NonDryRunBuildRejectsCompilerWithoutRequiredCompilePlanVersion)
{
  const fs::path root = MakeTempDir("build-contract-gate");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  WriteFile(
      root / "pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");

  const fs::path fake_styio = root / "fake-styio";
  WriteExecutable(
      fake_styio,
      "#!/bin/sh\n"
      "if [ \"$1\" = \"--machine-info=json\" ]; then\n"
      "  printf '%s\\n' '{\"tool\":\"styio\",\"compiler_version\":\"0.0.5\",\"channel\":\"stable\",\"supported_contracts\":{\"compile_plan\":[]},\"capabilities\":[\"machine_info_json\",\"single_file_entry\",\"jsonl_diagnostics\"],\"edition_max\":\"2026\"}'\n"
      "  exit 0\n"
      "fi\n"
      "echo unexpected invocation >&2\n"
      "exit 64\n");

  const int exit_code = pafio::RunCli({
      "build",
      "--manifest-path",
      (root / "pafio.toml").string(),
      "--styio-bin",
      fake_styio.string(),
  });
  EXPECT_EQ(exit_code, pafio::kExitContract);
}

TEST(BuildCliTests, NonDryRunBuildExecutesPublishedCompilePlan)
{
  const fs::path root = MakeTempDir("build-compile-plan-live");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  WriteFile(
      root / "pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");

  const fs::path fake_styio = root / "fake-styio";
  WriteFakeCompilePlanStyio(fake_styio);

  testing::internal::CaptureStdout();
  const int exit_code = pafio::RunCli({
      "--json",
      "build",
      "--manifest-path",
      (root / "pafio.toml").string(),
      "--styio-bin",
      fake_styio.string(),
  });
  const std::string stdout_text = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, pafio::kExitSuccess);
  const json payload = json::parse(stdout_text);
  EXPECT_EQ(payload.at("action").get<std::string>(), "build");
  EXPECT_EQ(payload.at("status").get<std::string>(), "succeeded");
  EXPECT_EQ(payload.at("mode").get<std::string>(), "execute");
  EXPECT_EQ(payload.at("intent").get<std::string>(), "build");
  EXPECT_EQ(payload.at("sync").at("status").get<std::string>(), "succeeded");
  EXPECT_EQ(payload.at("styio").at("integration_phase").get<std::string>(), "compile-plan-live");
  EXPECT_EQ(payload.at("styio").at("process").at("status").get<std::string>(), "exited");
  EXPECT_EQ(payload.at("styio").at("process").at("exit_code").get<int>(), 0);
  EXPECT_EQ(payload.at("styio").at("supported_compile_plan_versions").at(0).get<int>(), 1);

  const fs::path build_root = payload.at("plan").at("build_root").get<std::string>();
  ASSERT_TRUE(fs::exists(build_root / "receipt.json"));
  const json receipt = json::parse(ReadFile(build_root / "receipt.json"));
  EXPECT_EQ(receipt.at("tool").get<std::string>(), "styio");
  EXPECT_EQ(receipt.at("intent").get<std::string>(), "build");
}

TEST(BuildCliTests, SyncCompletesBeforeStyioProbeAndExplicitBinaryWinsOverEnvironment)
{
  const fs::path root = MakeTempDir("build-sync-before-styio");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  WriteFile(
      root / "pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", path = \"vendor/util\" }\n");
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");
  WriteFile(
      root / "vendor/util/pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/util\"\n"
      "version = \"0.2.0\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n");
  WriteFile(root / "vendor/util/src/lib.styio", "# util := true\n");

  const fs::path explicit_marker = root / "explicit-probed";
  const fs::path environment_marker = root / "environment-probed";
  const fs::path explicit_styio = root / "explicit-styio";
  const fs::path environment_styio = root / "environment-styio";
  WriteExecutable(
      environment_styio,
      "#!/bin/sh\n"
      "printf '%s\\n' environment > \"" + environment_marker.string() + "\"\n"
      "exit 70\n");
  WriteExecutable(
      explicit_styio,
      "#!/bin/sh\n"
      "if [ \"$1\" = \"--machine-info=json\" ]; then\n"
      "  test -f \"" + (root / "pafio.lock").string() + "\" || exit 71\n"
      "  test -f \"" + (root / ".pafio/resolution-v1.json").string() + "\" || exit 72\n"
      "  printf '%s\\n' explicit > \"" + explicit_marker.string() + "\"\n"
      "  printf '%s\\n' '{\"tool\":\"styio\",\"compiler_version\":\"0.0.5\",\"channel\":\"stable\",\"supported_contracts\":{\"compile_plan\":[1]},\"capabilities\":[\"machine_info_json\",\"single_file_entry\",\"jsonl_diagnostics\"],\"edition_max\":\"2026\"}'\n"
      "  exit 0\n"
      "fi\n" +
          pafio::testsupport::FakeCompilePlanConsumerBody() +
          "exit 64\n");
  const ScopedEnvVar environment_styio_bin("PAFIO_STYIO_BIN", environment_styio.string());

  testing::internal::CaptureStdout();
  const int exit_code = pafio::RunCli({
      "--json",
      "build",
      "--manifest-path",
      (root / "pafio.toml").string(),
      "--styio-bin",
      explicit_styio.string(),
  });
  const std::string stdout_text = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, pafio::kExitSuccess);
  const json payload = json::parse(stdout_text);
  EXPECT_EQ(payload.at("action").get<std::string>(), "build");
  EXPECT_EQ(payload.at("status").get<std::string>(), "succeeded");
  EXPECT_EQ(payload.at("intent").get<std::string>(), "build");
  EXPECT_EQ(payload.at("sync").at("status").get<std::string>(), "succeeded");
  EXPECT_EQ(payload.at("styio").at("process").at("status").get<std::string>(), "exited");
  EXPECT_EQ(payload.at("styio").at("process").at("exit_code").get<int>(), 0);
  EXPECT_TRUE(fs::exists(root / "pafio.lock"));
  EXPECT_TRUE(fs::exists(root / ".pafio/resolution-v1.json"));
  EXPECT_TRUE(fs::exists(explicit_marker));
  EXPECT_FALSE(fs::exists(environment_marker));
}
