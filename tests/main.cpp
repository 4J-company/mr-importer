#include <gtest/gtest.h>
#include <mr-importer/importer.hpp>

#include <filesystem>

namespace fs = std::filesystem;

TEST(UsdImport, TriangleMesh)
{
  fs::path const usd = fs::path(__FILE__).parent_path() / "data" / "triangle.usda";
  auto model = mr::importer::import(usd, mr::importer::Options::None);
  ASSERT_TRUE(model.has_value());
  ASSERT_FALSE(model->meshes.empty());
  EXPECT_GE(model->meshes.front().indices.size(), 3u);
  EXPECT_FALSE(model->materials.empty());
}
