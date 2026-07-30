#include "SpioResolve/MetadataContract.hpp"

#include <array>
#include <set>
#include <string>

#include <gtest/gtest.h>

TEST(MetadataTests, SerializerExposesOnlyTheMetadataV1TopLevelFields)
{
  const spio::MetadataDocument document{
      .package = {{"name", "acme/app"}},
      .workspace = {{"root", "."}},
      .dependencies = nlohmann::json::array({{{"name", "acme/util"}}}),
      .targets = nlohmann::json::array({{{"name", "app"}, {"kind", "bin"}}}),
      .lock = {{"present", true}},
      .resolution = {{"packages", 2}},
      .vendor = {{"present", false}},
  };

  const nlohmann::json payload = spio::SerializeMetadataV1(document);
  constexpr std::array<const char *, 7> expected{
      "package",
      "workspace",
      "dependencies",
      "targets",
      "lock",
      "resolution",
      "vendor",
  };

  ASSERT_EQ(payload.size(), expected.size());
  std::set<std::string> actual_fields;
  for (const auto &[field, value] : payload.items())
  {
    (void) value;
    actual_fields.insert(field);
  }
  EXPECT_EQ(actual_fields, std::set<std::string>(expected.begin(), expected.end()));

  for (const char *field : expected)
  {
    EXPECT_TRUE(payload.contains(field)) << field;
  }

  EXPECT_EQ(payload.at("package"), document.package);
  EXPECT_EQ(payload.at("workspace"), document.workspace);
  EXPECT_EQ(payload.at("dependencies"), document.dependencies);
  EXPECT_EQ(payload.at("targets"), document.targets);
  EXPECT_EQ(payload.at("lock"), document.lock);
  EXPECT_EQ(payload.at("resolution"), document.resolution);
  EXPECT_EQ(payload.at("vendor"), document.vendor);

  for (const char *forbidden : {"cloud", "cloud_policy", "toolchain", "managed_toolchain", "ide", "ide_hints"})
  {
    EXPECT_FALSE(payload.contains(forbidden)) << forbidden;
  }
}
