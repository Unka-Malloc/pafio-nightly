#include "BuildTestSupport.hpp"

#include "PafioCLI/CLI.hpp"
#include "PafioCompat/Compat.hpp"
#include "PafioCore/Errors.hpp"

#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using pafio::CompilerSelectionSource;
using namespace pafio::testsupport;

namespace
{

json MachineInfo()
{
  return {
      {"tool", "styio"},
      {"compiler_version", "0.9.7"},
      {"channel", "local-experimental"},
      {"supported_contracts", {{"compile_plan", {1}}}},
      {"capabilities", {"machine_info_json", "single_file_entry", "jsonl_diagnostics"}},
      {"edition_max", "2026"},
  };
}

void WriteCompiler(const fs::path &path, const json &payload)
{
  WriteExecutable(path,
      "#!/bin/sh\n"
      "if [ \"$1\" = \"--machine-info=json\" ]; then\n"
      "  printf '%s\\n' '" + payload.dump() + "'\n"
      "  exit 0\n"
      "fi\n" + FakeCompilePlanConsumerBody() + "exit 64\n");
}

void WriteProject(const fs::path &root)
{
  WriteFile(root / "pafio.toml",
      "[pafio]\nmanifest-version = 1\n\n"
      "[package]\nname = \"acme/app\"\nversion = \"0.1.0\"\nedition = \"2026\"\npublish = false\n\n"
      "[build]\nimplicit-std = true\n\n"
      "[[bin]]\nname = \"app\"\npath = \"src/main.styio\"\n\n"
      "[[test]]\nname = \"suite\"\npath = \"tests/suite.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");
  WriteFile(root / "tests/suite.styio", ">_(\"test\")\n");
}

std::map<std::string, std::string> Snapshot(const fs::path &root)
{
  std::map<std::string, std::string> result;
  for (const auto &entry : fs::recursive_directory_iterator(root))
  {
    result[fs::relative(entry.path(), root).generic_string()] =
        entry.is_regular_file() ? ReadFile(entry.path()) : "<directory>";
  }
  return result;
}

}  // namespace

TEST(CompatTests, ExplicitSelectionAdmitsUnlistedProductWithoutCertifyingRelease)
{
  const auto root = MakeTempDir("compat-explicit-unlisted");
  const auto binary = root / "styio";
  WriteCompiler(binary, MachineInfo());
  for (auto source : {CompilerSelectionSource::CommandLine, CompilerSelectionSource::Environment})
  {
    const auto report = pafio::CheckCompilerCompatibility(binary, source);
    EXPECT_FALSE(report.published_support);
    EXPECT_EQ(report.compiler_version, "0.9.7");
    EXPECT_EQ(report.compiler_channel, "local-experimental");
    EXPECT_EQ(report.integration_phase, "compile-plan-live");
    EXPECT_EQ(report.supported_compile_plan_versions, std::vector<int>{1});
  }
  EXPECT_THROW(pafio::CheckCompilerCompatibility(binary), pafio::CompatibilityError);
}

TEST(CompatTests, KnownProductsRetainPathAdmission)
{
  const auto root = MakeTempDir("compat-published");
  const auto binary = root / "styio";
  for (const auto *channel : {"stable", "nightly"})
  {
    auto info = MachineInfo();
    info["compiler_version"] = "0.0.5";
    info["channel"] = channel;
    WriteCompiler(binary, info);
    const auto report = pafio::CheckCompilerCompatibility(binary);
    EXPECT_TRUE(report.published_support);
    EXPECT_EQ(report.selection_source, "path");
  }
}

TEST(CompatTests, ExplicitSelectionNeverWaivesMandatoryRuntimeRequirements)
{
  const auto root = MakeTempDir("compat-mandatory");
  const auto binary = root / "styio";
  std::vector<json> invalid;
  for (const auto *capability : {"machine_info_json", "single_file_entry", "jsonl_diagnostics"})
  {
    auto info = MachineInfo();
    auto &capabilities = info["capabilities"];
    capabilities.erase(std::remove(capabilities.begin(), capabilities.end(), capability), capabilities.end());
    invalid.push_back(info);
  }
  auto info = MachineInfo();
  info["supported_contracts"]["compile_plan"] = json::array();
  invalid.push_back(info);  // Nano-style handshake remains rejected.
  info["supported_contracts"]["compile_plan"] = {2};
  invalid.push_back(info);
  info = MachineInfo();
  info["edition_max"] = "2025";
  invalid.push_back(info);
  for (const auto &payload : invalid)
  {
    SCOPED_TRACE(payload.dump());
    WriteCompiler(binary, payload);
    EXPECT_THROW(pafio::CheckCompilerCompatibility(binary, CompilerSelectionSource::CommandLine), pafio::CompatibilityError);
  }
}

TEST(CompatTests, MalformedHandshakeAlwaysRaisesContractError)
{
  const auto root = MakeTempDir("compat-malformed");
  const auto binary = root / "styio";
  std::vector<json> invalid{json::array(), nullptr};
  for (const auto *version : {"unknown", "0.9", "0.9.7.1", "0.9.7.", "0.9.7-dev", "00.9.7", "-1.9.7", "999999999999999999999999.0.0"})
  {
    auto info = MachineInfo();
    info["compiler_version"] = version;
    invalid.push_back(info);
  }
  for (const auto *field : {"tool", "compiler_version", "channel", "edition_max", "capabilities", "supported_contracts"})
  {
    auto info = MachineInfo();
    info.erase(field);
    invalid.push_back(info);
    info = MachineInfo();
    info[field] = 42;
    invalid.push_back(info);
  }
  for (const auto &value : {json(""), json("2026junk"), json(" 2026"), json("+2026")})
  {
    auto info = MachineInfo();
    info["edition_max"] = value;
    invalid.push_back(info);
  }
  for (const auto &value : {json({1.0}), json({true}), json({"1"}), json({2147483648ULL}), json(nullptr)})
  {
    auto info = MachineInfo();
    info["supported_contracts"]["compile_plan"] = value;
    invalid.push_back(info);
  }
  auto info = MachineInfo();
  info["channel"] = "";
  invalid.push_back(info);
  info = MachineInfo();
  info["capabilities"].push_back(42);
  invalid.push_back(info);
  for (const auto &payload : invalid)
  {
    SCOPED_TRACE(payload.dump());
    WriteCompiler(binary, payload);
    EXPECT_THROW(pafio::CheckCompilerCompatibility(binary, CompilerSelectionSource::CommandLine), pafio::CompatibilityError);
  }
}

TEST(CompatTests, DiscoveryPreservesExplicitSourceAndNeverFallsBack)
{
  const auto root = MakeTempDir("compat-discovery");
  const auto binary = root / "styio";
  WriteCompiler(binary, MachineInfo());
  const ScopedEnvVar path("PATH", root.string());
  {
    const ScopedEnvVar env("PAFIO_STYIO_BIN", "");
    const auto selected = pafio::ResolveStyioBinary(std::nullopt);
    ASSERT_TRUE(selected);
    EXPECT_EQ(selected->source, CompilerSelectionSource::Path);
  }
  const ScopedEnvVar env("PAFIO_STYIO_BIN", binary.string());
  const auto from_env = pafio::ResolveStyioBinary(std::nullopt);
  ASSERT_TRUE(from_env);
  EXPECT_EQ(from_env->source, CompilerSelectionSource::Environment);
  EXPECT_THROW(pafio::ResolveStyioBinary(std::string{}), pafio::CompilerProbeError);
  const auto explicit_missing = pafio::ResolveStyioBinary((root / "missing").string());
  ASSERT_TRUE(explicit_missing);
  EXPECT_EQ(explicit_missing->source, CompilerSelectionSource::CommandLine);
  EXPECT_EQ(explicit_missing->binary, root / "missing");
  EXPECT_THROW(pafio::CheckCompilerCompatibility(explicit_missing->binary, explicit_missing->source), pafio::CompilerProbeError);
}

TEST(CompatTests, DoctorWarnsForUnlistedSelectionWithoutMutatingProject)
{
  const auto root = MakeTempDir("compat-doctor-read-only");
  const ScopedEnvVar home("PAFIO_HOME", (root / "home").string());
  WriteProject(root);
  const auto binary = root / "styio";
  WriteCompiler(binary, MachineInfo());
  const auto before = Snapshot(root);
  testing::internal::CaptureStdout();
  pafio::RunCli({"--json", "doctor", "--manifest-path", (root / "pafio.toml").string(), "--styio-bin", binary.string()});
  const auto payload = json::parse(testing::internal::GetCapturedStdout());
  bool found = false;
  for (const auto &check : payload.at("checks"))
  {
    if (check.at("name") != "styio") continue;
    found = true;
    EXPECT_EQ(check.at("status"), "warning");
    EXPECT_EQ(check.at("detail").at("product_support"), "unlisted");
    EXPECT_EQ(check.at("detail").at("selection_source"), "command_line");
    EXPECT_EQ(check.at("detail").at("release_provenance"), "unverified");
    EXPECT_EQ(check.at("detail").at("supported_compile_plan_versions"), json({1}));
  }
  EXPECT_TRUE(found);
  EXPECT_EQ(Snapshot(root), before);
}

TEST(CompatTests, AllWorkflowsExecuteExplicitUnlistedCompilerAndPreserveChannel)
{
  for (const auto *command : {"check", "build", "run", "test"})
  {
    SCOPED_TRACE(command);
    const auto root = MakeTempDir(std::string("compat-workflow-") + command);
    const ScopedEnvVar home("PAFIO_HOME", (root / "home").string());
    WriteProject(root);
    const auto binary = root / "styio";
    WriteCompiler(binary, MachineInfo());
    testing::internal::CaptureStdout();
    const int code = pafio::RunCli({"--json", command, "--manifest-path", (root / "pafio.toml").string(), "--styio-bin", binary.string()});
    const std::string output = testing::internal::GetCapturedStdout();
    ASSERT_EQ(code, pafio::kExitSuccess) << output;
    const auto payload = json::parse(output);
    EXPECT_EQ(payload.at("styio").at("product_support"), "unlisted");
    EXPECT_EQ(payload.at("styio").at("selection_source"), "command_line");
    EXPECT_EQ(payload.at("styio").at("release_provenance"), "unverified");
    const auto plan = json::parse(ReadFile(payload.at("plan").at("path").get<std::string>()));
    EXPECT_EQ(plan.at("toolchain").at("channel"), "local-experimental");
    EXPECT_EQ(plan.at("toolchain").at("std_package_id"), "builtin:std@local-experimental/2026");
  }
}

