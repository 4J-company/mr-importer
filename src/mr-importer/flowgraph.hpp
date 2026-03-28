#pragma once

#include <taskflow/taskflow.hpp>

#include "mr-importer/options.hpp"

#include <filesystem>
#include <memory>
#include <optional>

// forward declaration
namespace fastgltf {
class Asset;
}

namespace mr {
inline namespace importer {
struct Model;

struct FlowGraph {
  std::optional<fastgltf::Asset> asset;
  std::unique_ptr<Model> model;
  std::filesystem::path path;
};

struct LoaderTasks {
  tf::Task load_asset;
  tf::Task load_meshes;
};

LoaderTasks add_loader_nodes(tf::Taskflow &tf, FlowGraph &graph, const Options &options);

void add_optimizer_nodes(
    tf::Taskflow &tf, FlowGraph &graph, const Options &options, tf::Task after_meshes);

namespace taskflow_exec {
tf::Executor &import_executor();
}

} // namespace importer
} // namespace mr
