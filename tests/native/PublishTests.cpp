#include "SpioCLI/CLI.hpp"
#include "SpioCore/Errors.hpp"
#include "SpioPublish/Publish.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{

fs::path MakeTempDir(const std::string &label)
{
  const fs::path root = fs::temp_directory_path() / "spio-native-publish-tests" / label;
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

void WriteFile(const fs::path &path, const std::string &content)
{
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  ASSERT_TRUE(out.good());
  out << content;
  ASSERT_TRUE(out.good());
}

}  // namespace

TEST(PublishTests, DryRunPreparesArchiveForPublishablePackage)
{
  const fs::path root = MakeTempDir("publishable-package");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  testing::internal::CaptureStdout();
  const int exit_code = spio::RunCli({
      "--json",
      "publish",
      "--manifest-path",
      (root / "spio.toml").string(),
      "--dry-run",
  });
  const std::string stdout_text = testing::internal::GetCapturedStdout();

  EXPECT_EQ(exit_code, spio::kExitSuccess);
  const json payload = json::parse(stdout_text);
  EXPECT_EQ(payload.at("command").get<std::string>(), "publish");
  EXPECT_EQ(payload.at("mode").get<std::string>(), "dry-run");
  EXPECT_EQ(payload.at("package").get<std::string>(), "acme/app");
  EXPECT_EQ(payload.at("dependencies").get<size_t>(), 0U);
  EXPECT_EQ(payload.at("dev_dependencies").get<size_t>(), 0U);
  EXPECT_TRUE(fs::exists(payload.at("archive_path").get<std::string>()));
}

TEST(PublishTests, RejectsPublishFalsePackage)
{
  const fs::path root = MakeTempDir("publish-false");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
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
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_THROW(
      spio::PreparePublishCandidate({
          .manifest_path = root / "spio.toml",
      }),
      spio::PublishError);
}

TEST(PublishTests, RejectsNonRegistryDependenciesForPublish)
{
  const fs::path root = MakeTempDir("publish-with-deps");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", path = \"vendor/util\" }\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_THROW(
      spio::PreparePublishCandidate({
          .manifest_path = root / "spio.toml",
      }),
      spio::PublishError);
}

TEST(PublishTests, AllowsRegistryDependenciesForPublish)
{
  const fs::path root = MakeTempDir("publish-with-registry-deps");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", version = \"0.2.0\", registry = \"https://packages.example.test\" }\n\n"
      "[dev-dependencies]\n"
      "fixture = { package = \"acme/fixture\", version = \"1.0.0\", registry = \"https://packages.example.test\" }\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  const spio::PublishResult result = spio::PreparePublishCandidate({
      .manifest_path = root / "spio.toml",
  });
  EXPECT_EQ(result.package_name, "acme/app");
  EXPECT_EQ(result.dependencies.size(), 1U);
  EXPECT_EQ(result.dev_dependencies.size(), 1U);
  EXPECT_TRUE(fs::exists(result.archive_path));
}

TEST(PublishTests, RemoteRequestCarriesBoundedArchiveContentWithoutLocalPaths)
{
  const fs::path root = MakeTempDir("remote-request");
  const fs::path archive_path = root / "acme-app-0.1.0.pafio.src.tar";
  WriteFile(archive_path, "archive");

  const json request = json::parse(spio::BuildRemotePublishRequestJson({
      .archive_path = archive_path,
      .package_name = "acme/app",
      .package_version = "0.1.0",
  }));

  EXPECT_EQ(request.size(), 9U);
  EXPECT_EQ(request.at("package").get<std::string>(), "acme/app");
  EXPECT_EQ(request.at("version").get<std::string>(), "0.1.0");
  EXPECT_EQ(request.at("archive_name").get<std::string>(), "acme-app-0.1.0.pafio.src.tar");
  EXPECT_EQ(request.at("archive_base64").get<std::string>(), "YXJjaGl2ZQ==");
  EXPECT_EQ(
      request.at("archive_sha256").get<std::string>(),
      "0eb3e36bfb24dcd9bb1d1bece1531216b59539a8fde17ee80224af0653c92aa3");
  EXPECT_EQ(request.at("archive_size_bytes").get<std::uintmax_t>(), 7U);
  EXPECT_EQ(request.at("publisher_id").get<std::string>(), "pafio-cli");
  EXPECT_TRUE(request.at("dependencies").empty());
  EXPECT_TRUE(request.at("dev_dependencies").empty());
  EXPECT_FALSE(request.contains("archive_path"));
  EXPECT_FALSE(request.contains("manifest_path"));
  EXPECT_FALSE(request.contains("package_root"));
}

TEST(PublishTests, RemoteRequestRejectsArchiveAboveExplicitLimit)
{
  const fs::path root = MakeTempDir("remote-request-limit");
  const fs::path archive_path = root / "package.pafio.src.tar";
  WriteFile(archive_path, "1234");

  EXPECT_THROW(
      spio::BuildRemotePublishRequestJson(
          {
              .archive_path = archive_path,
              .package_name = "acme/app",
              .package_version = "0.1.0",
          },
          3U),
      spio::PublishError);
}

TEST(PublishTests, RemoteRequestSerializesCanonicalDependencyMetadata)
{
  const fs::path root = MakeTempDir("remote-request-dependencies");
  const fs::path archive_path = root / "package.pafio.src.tar";
  WriteFile(archive_path, "archive");

  const json request = json::parse(spio::BuildRemotePublishRequestJson({
      .archive_path = archive_path,
      .package_name = "acme/app",
      .package_version = "0.1.0",
      .dependencies = {
          {
              .alias = "zeta",
              .package = "acme/zeta",
              .source_kind = spio::DependencySourceKind::kRegistry,
              .source = "https://packages.example.test",
              .version = "2.0.0",
          },
          {
              .alias = "alpha",
              .package = "acme/alpha",
              .source_kind = spio::DependencySourceKind::kRegistry,
              .source = "https://packages.example.test",
              .version = "1.0.0",
          },
      },
      .dev_dependencies = {
          {
              .alias = "fixture",
              .package = "acme/fixture",
              .source_kind = spio::DependencySourceKind::kRegistry,
              .source = "https://packages.example.test",
              .version = "3.0.0",
          },
      },
  }));

  ASSERT_EQ(request.at("dependencies").size(), 2U);
  EXPECT_EQ(
      request.at("dependencies")[0],
      json({
          {"alias", "alpha"},
          {"package", "acme/alpha"},
          {"version_req", "1.0.0"},
          {"registry", "https://packages.example.test"},
      }));
  EXPECT_EQ(request.at("dependencies")[1].at("alias").get<std::string>(), "zeta");
  ASSERT_EQ(request.at("dev_dependencies").size(), 1U);
  EXPECT_EQ(request.at("dev_dependencies")[0].at("alias").get<std::string>(), "fixture");
}

TEST(PublishTests, RemoteRequestRejectsUnboundedDependencyTables)
{
  const fs::path root = MakeTempDir("remote-request-dependency-limit");
  const fs::path archive_path = root / "package.pafio.src.tar";
  WriteFile(archive_path, "archive");

  spio::PublishResult candidate{
      .archive_path = archive_path,
      .package_name = "acme/app",
      .package_version = "0.1.0",
  };
  candidate.dependencies.resize(257U);

  EXPECT_THROW(spio::BuildRemotePublishRequestJson(candidate), spio::PublishError);
}

TEST(PublishTests, WorkspacePublishRequiresExplicitPackageSelectionWhenAmbiguous)
{
  const fs::path root = MakeTempDir("workspace-publish-ambiguity");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[workspace]\n"
      "members = [\"packages/app\", \"packages/tool\"]\n"
      "resolver = \"1\"\n");
  WriteFile(
      root / "packages/app/spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "packages/app/src/main.styio", ">_(\"app\")\n");
  WriteFile(
      root / "packages/tool/spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/tool\"\n"
      "version = \"0.2.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"tool\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "packages/tool/src/main.styio", ">_(\"tool\")\n");

  EXPECT_THROW(
      spio::PreparePublishCandidate({
          .manifest_path = root / "spio.toml",
      }),
      spio::PublishError);

  const spio::PublishResult result = spio::PreparePublishCandidate({
      .manifest_path = root / "spio.toml",
      .package_name = "acme/tool",
  });
  EXPECT_EQ(result.package_name, "acme/tool");
  EXPECT_TRUE(fs::exists(result.archive_path));
}

TEST(PublishCliTests, NonDryRunPublishRequiresExplicitRegistryRoot)
{
  const fs::path root = MakeTempDir("publish-missing-registry");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
            }),
            spio::kExitUsage);
}

TEST(PublishCliTests, RejectsFilesystemRegistryBeforePreparingArchive)
{
  const fs::path root = MakeTempDir("publish-filesystem-before-pack");
  EXPECT_EQ(
      spio::RunCli({
          "publish",
          "--manifest-path",
          (root / "missing.toml").string(),
          "--registry",
          (root / "registry").string(),
      }),
      spio::kExitUsage);
}

TEST(PublishCliTests, RejectsFilesystemRegistryPath)
{
  const fs::path root = MakeTempDir("publish-filesystem-registry");
  const fs::path registry_root = root / "registry";
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_EQ(
      spio::RunCli({
          "publish",
          "--manifest-path",
          (root / "spio.toml").string(),
          "--registry",
          registry_root.string(),
      }),
      spio::kExitUsage);
}

TEST(PublishCliTests, RejectsFilesystemRegistryFileUrl)
{
  const fs::path root = MakeTempDir("publish-filesystem-registry-file-url");
  const fs::path registry_root = root / "registry";
  const std::string registry_url = std::string("file://") + registry_root.string();
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_EQ(
      spio::RunCli({
          "publish",
          "--manifest-path",
          (root / "spio.toml").string(),
          "--registry",
          registry_url,
      }),
      spio::kExitUsage);
}

TEST(PublishCliTests, RejectsRegistryHeaderInDryRun)
{
  const fs::path root = MakeTempDir("publish-dry-run-header");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
                "--dry-run",
                "--registry-header",
                "X-Spio-Write-Token: dev-token",
            }),
            spio::kExitUsage);
}

TEST(PublishCliTests, RejectsMalformedRegistryHeader)
{
  const fs::path root = MakeTempDir("publish-malformed-header");
  const std::string registry_url = "https://packages.example.test";
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
                "--registry",
                registry_url,
                "--registry-header",
                "X-Spio-Write-Token",
            }),
            spio::kExitUsage);
}

TEST(PublishCliTests, RejectsRegistryHeaderForRemoteRegistryWithoutPrivateSecurityModule)
{
  const fs::path root = MakeTempDir("publish-remote-header-requires-private");
  const std::string registry_url = "https://packages.example.test";
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
                "--registry",
                registry_url,
                "--registry-header",
                "X-Spio-Write-Token: dev-token",
            }),
            spio::kExitPublish);
}

TEST(PublishCliTests, RejectsRegistryPolicyFileInDryRun)
{
  const fs::path root = MakeTempDir("publish-dry-run-policy");
  const fs::path policy_path = root / "publish-policy.toml";
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");
  WriteFile(
      policy_path,
      "schema-version = 1\n\n"
      "[[registry]]\n"
      "root = \"https://packages.example.test\"\n"
      "headers = [\"X-Spio-Write-Token: dev-token\"]\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
                "--dry-run",
                "--registry-policy-file",
                policy_path.string(),
            }),
            spio::kExitUsage);
}

TEST(PublishCliTests, RejectsRegistryPolicyFileForRemoteRegistryWithoutPrivateSecurityModule)
{
  const fs::path root = MakeTempDir("publish-remote-policy-requires-private");
  const std::string registry_url = "https://packages.example.test";
  const fs::path policy_path = root / "publish-policy.toml";
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");
  WriteFile(
      policy_path,
      "schema-version = 2\n\n"
      "[[registry]]\n"
      "root = \"https://packages.example.test\"\n"
      "headers = [\"X-Spio-Write-Token: dev-token\"]\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
                "--registry",
                registry_url,
                "--registry-policy-file",
                policy_path.string(),
            }),
            spio::kExitPublish);
}

TEST(PublishCliTests, RejectsRegistryProfileInDryRun)
{
  const fs::path root = MakeTempDir("publish-dry-run-profile");
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
                "--dry-run",
                "--registry-profile",
                "dev",
            }),
            spio::kExitUsage);
}

TEST(PublishCliTests, RejectsRegistryProfileForRemoteRegistryWithoutPrivateSecurityModule)
{
  const fs::path root = MakeTempDir("publish-remote-profile-requires-private");
  const fs::path spio_home = root / ".spio-home";
  const fs::path profile_path = spio_home / "server/registry/publish-profiles/dev.toml";
  const char *previous_spio_home = std::getenv("SPIO_HOME");
  const std::string previous_spio_home_value = previous_spio_home != nullptr ? previous_spio_home : "";
  setenv("SPIO_HOME", spio_home.string().c_str(), 1);

  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");
  WriteFile(
      profile_path,
      "schema-version = 1\n\n"
      "[[registry]]\n"
      "root = \"https://packages.example.test\"\n"
      "headers = [\"X-Spio-Write-Token: dev-token\"]\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
                "--registry",
                "https://packages.example.test",
                "--registry-profile",
                "dev",
            }),
            spio::kExitPublish);

  if (previous_spio_home != nullptr)
  {
    setenv("SPIO_HOME", previous_spio_home_value.c_str(), 1);
  }
  else
  {
    unsetenv("SPIO_HOME");
  }
}

TEST(PublishCliTests, RejectsRegistryProfileWhenPolicyFileAlsoProvided)
{
  const fs::path root = MakeTempDir("publish-profile-policy-conflict");
  const std::string registry_url = "https://packages.example.test";
  const fs::path policy_path = root / "publish-policy.toml";
  WriteFile(
      root / "spio.toml",
      "[spio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/main.styio", ">_(\"hello\")\n");
  WriteFile(
      policy_path,
      "schema-version = 1\n\n"
      "[[registry]]\n"
      "root = \"https://packages.example.test\"\n"
      "headers = [\"X-Spio-Write-Token: dev-token\"]\n");

  EXPECT_EQ(spio::RunCli({
                "publish",
                "--manifest-path",
                (root / "spio.toml").string(),
                "--registry",
                registry_url,
                "--registry-profile",
                "dev",
                "--registry-policy-file",
                policy_path.string(),
            }),
            spio::kExitUsage);
}
