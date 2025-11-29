#pragma once

#include "pch.hpp"

#include "mr-importer/options.hpp"

// forward declaration
namespace fastgltf {
class Asset;
}

namespace mr {
struct Model;

inline namespace importer {
struct FlowGraph {
  std::optional<fastgltf::Asset> asset;
  std::unique_ptr<Model> model;
  std::filesystem::path path;

  tbb::flow::graph graph;

  std::unique_ptr<tbb::flow::input_node<fastgltf::Asset *>> asset_loader;

  std::unique_ptr<
      tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>>
      meshes_load;
  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *>> materials_load;
  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *>> lights_load;

  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>> meshes_optimize;
  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>> mesh_lod_generate;
  std::unique_ptr<tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>> meshlet_generate;
};

void add_loader_nodes(FlowGraph &graph, const Options &options);
void add_optimizer_nodes(FlowGraph &graph, const Options &options);
} // namespace importer
} // namespace mr
