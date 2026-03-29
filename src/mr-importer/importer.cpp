#include "mr-importer/importer.hpp"

#include "pch.hpp"

#include <algorithm>
#include <cctype>

#include "flowgraph.hpp"

namespace mr {
inline namespace importer {
namespace {
bool is_usd_extension(std::filesystem::path const &path)
{
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return ext == ".usd" || ext == ".usda" || ext == ".usdc" || ext == ".usdz";
}
} // namespace
/**
 * \brief High-level import entry point.
 *
 * Loads an asset from disk, optionally optimizes meshes, and returns the
 * result.
 * \param path Path to a source asset (e.g. glTF file).
 * \param options Import behavior flags, see \ref Options.
 * \return Imported \ref Model or std::nullopt if loading failed.
 */
std::optional<Model> import(const std::filesystem::path &path, Options options)
{
  ZoneScoped;

  if (path.extension() == ".mrmodel") {
    return deserialize(path.string());
  }

  FlowGraph graph;
  graph.path = std::move(path);

  if (is_usd_extension(graph.path)) {
    add_usd_loader_nodes(graph, options);
  }
  else {
    add_gltf_loader_nodes(graph, options);
  }
  add_optimizer_nodes(graph, options);

  graph.asset_loader->activate();
  graph.graph.wait_for_all();

  if (!graph.model) {
    return std::nullopt;
  }

  return std::move(*graph.model.get());
}
} // namespace importer
} // namespace mr
