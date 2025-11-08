#include "mr-importer/importer.hpp"

#include "pch.hpp"

#include "flowgraph.hpp"

namespace mr {
inline namespace importer {
  /**
   * \brief High-level import entry point.
   *
   * Loads an asset from disk, optionally optimizes meshes, and returns the result.
   * \param path Path to a source asset (e.g. glTF file).
   * \param options Import behavior flags, see \ref Options.
   * \return Imported \ref Model or std::nullopt if loading failed.
   */
  std::optional<Model> import(const std::filesystem::path& path, Options options)
  {
    FlowGraph graph;
    graph.path = std::move(path);

    add_loader_nodes(graph, options);
    add_optimizer_nodes(graph, options);

    graph.asset_loader->activate();
    graph.graph.wait_for_all();

    return std::move(*graph.model.get());
  }
} // namespace importer
} // namespace mr
