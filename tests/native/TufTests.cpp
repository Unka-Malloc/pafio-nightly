#include "SpioRegistryClient/Client.hpp"
#include "SpioSecurity/TufVerifier.hpp"
#include "BuildTestSupport.hpp"

#include "SpioCore/Errors.hpp"
#include "SpioCore/Sha256.hpp"

#include <filesystem>
#include <map>
#include <string>

#include <gtest/gtest.h>

namespace fs = std::filesystem;
using spio::testsupport::MakeTempDir;
using spio::testsupport::ReadFile;
using spio::testsupport::ScopedEnvVar;
using spio::testsupport::WriteFile;

namespace
{

std::map<std::string, std::string> LoadTreeObjects(const fs::path &root)
{
  std::map<std::string, std::string> objects;
  for (const auto &entry : fs::recursive_directory_iterator(root))
  {
    if (!entry.is_regular_file())
    {
      continue;
    }
    const fs::path relative = fs::relative(entry.path(), root);
    objects[relative.generic_string()] = ReadFile(entry.path());
  }
  return objects;
}

}  // namespace

// These tests expect a published registry fixture under SPIO_TUF_FIXTURE_ROOT
// (produced by tests/unit/test_tuf_parity.py). When unset, they are skipped.
TEST(TufVerifierTests, AcceptsValidFixtureAndRejectsTamperedRoles)
{
  const char *fixture_env = std::getenv("SPIO_TUF_FIXTURE_ROOT");
  if (fixture_env == nullptr || *fixture_env == '\0')
  {
    GTEST_SKIP() << "Set SPIO_TUF_FIXTURE_ROOT to a published registry-v2 fixture";
  }
  const fs::path fixture_root = fixture_env;
  const auto objects = LoadTreeObjects(fixture_root);
  const spio::TufVerifyResult ok = spio::VerifyTufChainForPackage(objects, "acme/util", "acme");
  ASSERT_TRUE(ok.ok) << ok.error_code << ": " << ok.error;

  for (const char *relative : {"trust/root.json", "trust/timestamp.json", "trust/snapshot.json", "trust/targets/acme.json"})
  {
    auto tampered = objects;
    tampered[relative] = tampered[relative] + "\n";
    const spio::TufVerifyResult failed = spio::VerifyTufChainForPackage(tampered, "acme/util", "acme");
    EXPECT_FALSE(failed.ok) << relative;
    EXPECT_FALSE(failed.error_code.empty()) << relative;
  }
}

TEST(TufVerifierTests, MaterializeFailsClosedOnTamperedRootWithoutCacheWrite)
{
  const char *fixture_env = std::getenv("SPIO_TUF_FIXTURE_ROOT");
  if (fixture_env == nullptr || *fixture_env == '\0')
  {
    GTEST_SKIP() << "Set SPIO_TUF_FIXTURE_ROOT to a published registry-v2 fixture";
  }
  const fs::path root = MakeTempDir("tuf-materialize-tamper");
  const ScopedEnvVar spio_home("SPIO_HOME", (root / ".spio-home").string());
  const fs::path registry = root / "registry";
  fs::copy(fixture_env, registry, fs::copy_options::recursive);
  WriteFile(registry / "trust" / "timestamp.json", ReadFile(registry / "trust" / "timestamp.json") + "\n");

  const std::string registry_root = std::string("file://") + fs::absolute(registry).generic_string();
  const fs::path blob_root = root / ".spio-home" / "cache" / "registry" / "blobs";
  EXPECT_THROW(
      spio::MaterializeRegistryPackage(registry_root, "acme/util", "1.0.0", false),
      spio::FetchError);
  EXPECT_TRUE(!fs::exists(blob_root) || fs::is_empty(blob_root));
}
