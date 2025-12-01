#pragma once

#include "pch.hpp"

#include "mr-importer/options.hpp"

// forward declaration
namespace fastgltf {
class Asset;
}

namespace mr {
inline namespace importer {
struct Model;

// clang-format off
struct FlowGraph {
  std::optional<fastgltf::Asset> asset;
  std::unique_ptr<Model> model;
  std::filesystem::path path;

  tbb::flow::graph graph;

  std::unique_ptr<tbb::flow::input_node<fastgltf::Asset *>> asset_loader;
  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>> meshes_load;
  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *>> materials_load;
  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *>> lights_load;

  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *, std::vector<size_t>>> split_meshes;
  std::unique_ptr<tbb::flow::broadcast_node<size_t>> mesh_index_broadcaster;
  std::unique_ptr<tbb::flow::function_node<size_t, size_t>> mesh_processor;
  std::unique_ptr<tbb::flow::join_node<std::tuple<size_t, fastgltf::Asset *>, tbb::flow::queueing>> meshes_join;
  std::unique_ptr<tbb::flow::function_node<std::tuple<size_t, fastgltf::Asset *>, fastgltf::Asset *>> continue_after_meshes;
  std::unique_ptr<tbb::flow::function_node<std::vector<size_t>, tbb::flow::continue_msg>> vector_splitter;
  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>> asset_replicator;
};
// clang-format on

void add_loader_nodes(FlowGraph &graph, const Options &options);
void add_optimizer_nodes(FlowGraph &graph, const Options &options);
} // namespace importer
} // namespace mr
