#include "BuildTestSupport.hpp"

#include "SpioCLI/CLI.hpp"
#include "SpioCore/Errors.hpp"
#include "SpioPlan/CompilePlan.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

using spio::testsupport::MakeTempDir;
using spio::testsupport::ScopedEnvVar;
using spio::testsupport::WriteExecutable;
using spio::testsupport::WriteFile;

TEST(RunCliTests, DryRunEmitsRunIntentForUniqueBinaryTarget)
{
  const fs::path root = MakeTempDir("run-dry-run");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");

  const spio::BuildPlanResult result = spio::WriteBuildCompilePlan({
      .manifest_path = root / "spio.toml",
      .intent = "run",
  });
  const json plan = json::parse(spio::testsupport::ReadFile(result.plan_path));
  EXPECT_EQ(plan["intent"], "run");
  EXPECT_EQ(plan["entry"]["target_kind"], "bin");
  EXPECT_EQ(plan["entry"]["target_name"], "app");
}

TEST(RunCliTests, RejectsLibSelection)
{
  const fs::path root = MakeTempDir("run-rejects-lib");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");

  const int exit_code = spio::RunCli({
      "--json",
      "run",
      "--manifest-path",
      (root / "spio.toml").string(),
      "--lib",
      "--dry-run",
  });
  EXPECT_EQ(exit_code, spio::kExitUsage);
}

TEST(RunCliTests, NonDryRunRunIsBlockedByPublishedCompatibilityPhase)
{
  const fs::path root = MakeTempDir("run-contract-gate");
  const ScopedEnvVar spio_home("SPIO_HOME", (root / ".spio-home").string());
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
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

  const int exit_code = spio::RunCli({
      "run",
      "--manifest-path",
      (root / "spio.toml").string(),
      "--styio-bin",
      fake_styio.string(),
  });
  EXPECT_EQ(exit_code, spio::kExitContract);
}

TEST(TestCliTests, DryRunEmitsTestIntentForUniqueTestTarget)
{
  const fs::path root = MakeTempDir("test-dry-run");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[[test]]\n"
      "name = \"smoke\"\n"
      "path = \"tests/smoke.styio\"\n");
  WriteFile(root / "tests/smoke.styio", "# smoke := true\n");

  const spio::BuildPlanResult result = spio::WriteBuildCompilePlan({
      .manifest_path = root / "spio.toml",
      .intent = "test",
  });
  const json plan = json::parse(spio::testsupport::ReadFile(result.plan_path));
  EXPECT_EQ(plan["intent"], "test");
  EXPECT_EQ(plan["entry"]["target_kind"], "test");
  EXPECT_EQ(plan["entry"]["target_name"], "smoke");
  ASSERT_TRUE(plan["packages"][0]["targets"].contains("tests"));
  EXPECT_EQ(plan["packages"][0]["targets"]["tests"][0]["name"], "smoke");
}

TEST(TestCliTests, RejectsBinSelection)
{
  const fs::path root = MakeTempDir("test-rejects-bin");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[[test]]\n"
      "name = \"smoke\"\n"
      "path = \"tests/smoke.styio\"\n");
  WriteFile(root / "tests/smoke.styio", "# smoke := true\n");

  const int exit_code = spio::RunCli({
      "--json",
      "test",
      "--manifest-path",
      (root / "spio.toml").string(),
      "--bin",
      "app",
      "--dry-run",
  });
  EXPECT_EQ(exit_code, spio::kExitUsage);
}

TEST(TestCliTests, NonDryRunTestIsBlockedByPublishedCompatibilityPhase)
{
  const fs::path root = MakeTempDir("test-contract-gate");
  const ScopedEnvVar spio_home("SPIO_HOME", (root / ".spio-home").string());
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[[test]]\n"
      "name = \"smoke\"\n"
      "path = \"tests/smoke.styio\"\n");
  WriteFile(root / "tests/smoke.styio", "# smoke := true\n");

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

  const int exit_code = spio::RunCli({
      "test",
      "--manifest-path",
      (root / "spio.toml").string(),
      "--styio-bin",
      fake_styio.string(),
  });
  EXPECT_EQ(exit_code, spio::kExitContract);
}
