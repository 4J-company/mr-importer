#pragma once

#include "pch.hpp"

#include "mr-importer/options.hpp"

namespace mr {
inline namespace importer {
struct Model;

/** Opaque sync token for the loader/optimizer TBB graph (not dereferenced for USD). */
inline void *usd_pipeline_token()
{
  return reinterpret_cast<void *>(uintptr_t{1});
}

// clang-format off
struct FlowGraph {
  std::optional<fastgltf::Asset> asset;
  std::unique_ptr<Model> model;
  std::filesystem::path path;

  tbb::flow::graph graph;

  std::unique_ptr<tbb::flow::input_node<void *>> asset_loader;
  std::unique_ptr<tbb::flow::function_node<void *, void *>> meshes_load;
  std::unique_ptr<tbb::flow::function_node<void *>> materials_load;
  std::unique_ptr<tbb::flow::function_node<void *>> lights_load;

  std::unique_ptr<tbb::flow::function_node<void *, std::vector<size_t>>> split_meshes;
  std::unique_ptr<tbb::flow::broadcast_node<size_t>> mesh_index_broadcaster;
  std::unique_ptr<tbb::flow::function_node<size_t, size_t>> mesh_processor;
  std::unique_ptr<tbb::flow::join_node<std::tuple<size_t, void *>, tbb::flow::queueing>> meshes_join;
  std::unique_ptr<tbb::flow::function_node<std::tuple<size_t, void *>, void *>> continue_after_meshes;
  std::unique_ptr<tbb::flow::function_node<std::vector<size_t>, tbb::flow::continue_msg>> vector_splitter;
  std::unique_ptr<tbb::flow::function_node<void *, void *>> asset_replicator;
};
// clang-format on

void add_gltf_loader_nodes(FlowGraph &graph, const Options &options);
void add_usd_loader_nodes(FlowGraph &graph, const Options &options);
void add_optimizer_nodes(FlowGraph &graph, const Options &options);
} // namespace importer
} // namespace mr
