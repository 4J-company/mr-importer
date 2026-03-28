#include "mr-importer/importer.hpp"

#include "pch.hpp"

#include "flowgraph.hpp"

namespace mr {
inline namespace importer {
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

  tf::Taskflow taskflow;
  LoaderTasks loader = add_loader_nodes(taskflow, graph, options);
  add_optimizer_nodes(taskflow, graph, options, loader.load_meshes);
  taskflow_exec::import_executor().run(taskflow).wait();

  if (!graph.model) {
    return std::nullopt;
  }

  return std::move(*graph.model.get());
}
} // namespace importer
} // namespace mr
