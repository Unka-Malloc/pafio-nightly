#include "BuildTestSupport.hpp"

#include "PafioCore/Errors.hpp"
#include "PafioPlan/CompilePlan.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

using pafio::testsupport::CanonicalAbsolutePath;
using pafio::testsupport::MakeTempDir;
using pafio::testsupport::ReadFile;
using pafio::testsupport::WriteFile;

TEST(BuildPlanTests, WritesCompilePlanForSingleLibPackage)
{
  const fs::path root = MakeTempDir("single-lib-dry-run");
  WriteFile(
      root / "pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/demo\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n");
  WriteFile(root / "src/lib.styio", "# value := 1\n");

  const pafio::BuildPlanResult result = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .select_lib = true,
  });

  EXPECT_TRUE(fs::exists(result.plan_path));
  EXPECT_EQ(result.entry_target_kind, "lib");
  EXPECT_EQ(result.entry_package_name, "acme/demo");

  const json plan = json::parse(ReadFile(result.plan_path));
  EXPECT_EQ(plan["plan_version"], 1);
  EXPECT_EQ(plan["intent"], "build");
  EXPECT_EQ(plan["workspace_root"], CanonicalAbsolutePath(root).string());
  EXPECT_EQ(plan["entry"]["target_kind"], "lib");
  EXPECT_EQ(plan["entry"]["file"], CanonicalAbsolutePath(root / "src/lib.styio").string());
  EXPECT_EQ(plan["toolchain"]["std_package_id"], "builtin:std@unbound/2026");
  EXPECT_EQ(plan["profile"]["name"], "dev");
  EXPECT_FALSE(plan["profile"].contains("build_mode"));
  EXPECT_EQ(plan["emit"]["error_format"], "jsonl");
  EXPECT_FALSE(plan["emit"].contains("observable_static_snapshot"));
  EXPECT_EQ(
      plan["emit"].dump(),
      R"({"ast":false,"error_format":"jsonl","llvm_ir":false,"styio_ir":false})");
  ASSERT_EQ(plan["packages"].size(), 1U);
  EXPECT_EQ(plan["packages"][0]["targets"]["lib"], CanonicalAbsolutePath(root / "src/lib.styio").string());
}

namespace
{

void WriteSingleBinProject(const fs::path &root)
{
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
}

}  // namespace

TEST(BuildPlanTests, EmitsObservableStaticSnapshotRequestOnlyWhenRequested)
{
  const fs::path root = MakeTempDir("observable-static-snapshot-request");
  WriteSingleBinProject(root);

  const pafio::BuildPlanResult baseline = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
  });
  const pafio::BuildPlanResult baseline_again = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
  });
  EXPECT_EQ(baseline.plan_json, baseline_again.plan_json);
  EXPECT_FALSE(json::parse(baseline.plan_json)["emit"].contains("observable_static_snapshot"));

  const pafio::BuildPlanResult requested = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
      .observable_static_snapshot = pafio::ObservableStaticSnapshotRequest{
          .schema_version = 1,
          .required_capabilities = {"zeta", "alpha", "zeta", "beta"},
      },
  });
  EXPECT_NE(requested.cache_key, baseline.cache_key);

  const json plan = json::parse(ReadFile(requested.plan_path));
  EXPECT_EQ(plan["intent"], "check");
  EXPECT_EQ(plan["emit"]["error_format"], "jsonl");
  EXPECT_EQ(plan["emit"]["ast"], false);
  EXPECT_EQ(plan["emit"]["styio_ir"], false);
  EXPECT_EQ(plan["emit"]["llvm_ir"], false);
  ASSERT_TRUE(plan["emit"].contains("observable_static_snapshot"));
  EXPECT_EQ(
      plan["emit"]["observable_static_snapshot"].dump(),
      R"({"required_capabilities":["alpha","beta","zeta"],"schema_version":1})");

  const pafio::BuildPlanResult defaults = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
      .observable_static_snapshot = pafio::ObservableStaticSnapshotRequest{},
  });
  EXPECT_EQ(
      json::parse(defaults.plan_json)["emit"]["observable_static_snapshot"].dump(),
      R"({"required_capabilities":[],"schema_version":1})");
}

TEST(BuildPlanTests, ObservableParentSnapshotPathIsEmittedOnlyWhenRequestedAndNeverChangesCacheKey)
{
  const fs::path root = MakeTempDir("observable-parent-snapshot");
  WriteSingleBinProject(root);

  const pafio::BuildPlanResult baseline = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
  });
  EXPECT_EQ(
      json::parse(baseline.plan_json)["emit"].dump(),
      R"({"ast":false,"error_format":"jsonl","llvm_ir":false,"styio_ir":false})");

  const pafio::BuildPlanResult without_parent = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
      .observable_static_snapshot = pafio::ObservableStaticSnapshotRequest{
          .schema_version = 1,
          .required_capabilities = {"alpha"},
      },
  });
  EXPECT_FALSE(
      json::parse(without_parent.plan_json)["emit"]["observable_static_snapshot"].contains("parent_snapshot_path"));

  const fs::path absolute_parent =
      CanonicalAbsolutePath(root / "previous" / "app.observable-static-snapshot.json");
  const pafio::BuildPlanResult with_parent = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
      .observable_static_snapshot = pafio::ObservableStaticSnapshotRequest{
          .schema_version = 1,
          .required_capabilities = {"alpha"},
          .parent_snapshot_path = root / "previous" / "." / "app.observable-static-snapshot.json",
      },
  });
  EXPECT_EQ(with_parent.cache_key, without_parent.cache_key);
  EXPECT_EQ(with_parent.build_root, without_parent.build_root);
  EXPECT_NE(with_parent.cache_key, baseline.cache_key);

  const json with_parent_request = json::parse(with_parent.plan_json)["emit"]["observable_static_snapshot"];
  EXPECT_EQ(with_parent_request["schema_version"], 1);
  EXPECT_EQ(with_parent_request["required_capabilities"].dump(), R"(["alpha"])");
  EXPECT_EQ(with_parent_request["parent_snapshot_path"], absolute_parent.string());
  EXPECT_EQ(
      with_parent_request.dump(),
      R"({"parent_snapshot_path":")" + absolute_parent.string() +
          R"(","required_capabilities":["alpha"],"schema_version":1})");

  const pafio::BuildPlanResult with_relative_parent = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
      .observable_static_snapshot = pafio::ObservableStaticSnapshotRequest{
          .schema_version = 1,
          .required_capabilities = {"alpha"},
          .parent_snapshot_path = fs::path("previous/app.observable-static-snapshot.json"),
      },
  });
  EXPECT_EQ(with_relative_parent.cache_key, without_parent.cache_key);
  EXPECT_EQ(
      json::parse(with_relative_parent.plan_json)["emit"]["observable_static_snapshot"]["parent_snapshot_path"],
      absolute_parent.string());

  const pafio::BuildPlanResult baseline_again = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .intent = "check",
  });
  EXPECT_EQ(baseline_again.plan_json, baseline.plan_json);

  EXPECT_THROW(
      pafio::WriteBuildCompilePlan({
          .manifest_path = root / "pafio.toml",
          .intent = "check",
          .observable_static_snapshot = pafio::ObservableStaticSnapshotRequest{
              .schema_version = 1,
              .parent_snapshot_path = fs::path(),
          },
      }),
      pafio::PlanError);
}

TEST(BuildPlanTests, RejectsMalformedObservableStaticSnapshotRequest)
{
  const fs::path root = MakeTempDir("observable-static-snapshot-invalid");
  WriteSingleBinProject(root);

  EXPECT_THROW(
      pafio::WriteBuildCompilePlan({
          .manifest_path = root / "pafio.toml",
          .intent = "check",
          .observable_static_snapshot = pafio::ObservableStaticSnapshotRequest{.schema_version = 0},
      }),
      pafio::PlanError);
  EXPECT_THROW(
      pafio::WriteBuildCompilePlan({
          .manifest_path = root / "pafio.toml",
          .intent = "check",
          .observable_static_snapshot = pafio::ObservableStaticSnapshotRequest{
              .schema_version = 1,
              .required_capabilities = {"alpha", ""},
          },
      }),
      pafio::PlanError);
}

TEST(BuildPlanTests, RejectsAmbiguousPackageTargetsWithoutExplicitSelection)
{
  const fs::path root = MakeTempDir("ambiguous-target");
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
      "[lib]\n"
      "path = \"src/lib.styio\"\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "src/lib.styio", "# lib := 1\n");
  WriteFile(root / "src/main.styio", ">_(\"hi\")\n");

  EXPECT_THROW(
      pafio::WriteBuildCompilePlan({
          .manifest_path = root / "pafio.toml",
      }),
      pafio::PlanError);
}

TEST(BuildPlanTests, WorkspaceBuildRequiresExplicitPackageSelectionWhenMultipleRootsExist)
{
  const fs::path root = MakeTempDir("workspace-package-selection");
  WriteFile(
      root / "pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[workspace]\n"
      "members = [\"packages/app\", \"packages/tool\"]\n"
      "resolver = \"1\"\n");
  WriteFile(
      root / "packages/app/pafio.toml",
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
  WriteFile(root / "packages/app/src/main.styio", ">_(\"app\")\n");
  WriteFile(
      root / "packages/tool/pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/tool\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"tool\"\n"
      "path = \"src/main.styio\"\n");
  WriteFile(root / "packages/tool/src/main.styio", ">_(\"tool\")\n");

  EXPECT_THROW(
      pafio::WriteBuildCompilePlan({
          .manifest_path = root / "pafio.toml",
      }),
      pafio::PlanError);

  const pafio::BuildPlanResult result = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .package_name = "acme/tool",
  });
  EXPECT_EQ(result.entry_package_name, "acme/tool");
  EXPECT_EQ(result.entry_target_kind, "bin");
  EXPECT_EQ(result.entry_target_name, "tool");
}

TEST(BuildPlanTests, RejectsMixedEditionGraphForCompilePlanV1)
{
  const fs::path root = MakeTempDir("mixed-edition");
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
      "util = { package = \"acme/util\", path = \"deps/util\" }\n");
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");
  WriteFile(
      root / "deps/util/pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/util\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2027\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n");
  WriteFile(root / "deps/util/src/lib.styio", "# util := 1\n");

  EXPECT_THROW(
      pafio::WriteBuildCompilePlan({
          .manifest_path = root / "pafio.toml",
      }),
      pafio::PlanError);
}
