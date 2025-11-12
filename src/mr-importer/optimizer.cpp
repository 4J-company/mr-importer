/**
 * \file optimizer.cpp
 * \brief Mesh optimization and multi-LOD generation implementation.
 */

#include "mr-importer/importer.hpp"

#include "pch.hpp"

#include "flowgraph.hpp"

namespace mr {
inline namespace importer {
namespace {
static std::pair<size_t, float> determine_lod_count_and_ratio(
    const PositionArray &positions, const IndexSpan &indices)
{
  ZoneScoped;

  constexpr int maxlods = 3;

  float lod_scale = meshopt_simplifyScale(
      (float *)positions.data(), positions.size(), sizeof(Position));
  size_t lod_count = 0;

  if (lod_scale > 1) {
    return {0, 0};
  }

  // we want any mesh to have at least 47 triangles
  lod_count = std::ceil(std::log(3 * 47 / indices.size()) / std::log(lod_scale));

  if (lod_count < 1) {
    return {0, 0};
  }

  if (lod_count > maxlods) {
    lod_scale = std::pow(lod_scale, lod_count / (float)maxlods);
    lod_count = maxlods;
  }

  return {lod_count, lod_scale};
}

static std::pair<IndexSpan, IndexSpan> generate_lod(
    const PositionArray &positions,
    const IndexSpan &original_indices,
    IndexArray &index_array,
    const std::span<meshopt_Stream> &streams,
    float lod_ratio,
    int lod_index)
{
  ZoneScoped;

  static constexpr float target_error = 0.05f;

  IndexArray result_indices;
  IndexArray result_shadow_indices;

  const float lod_scale = std::pow(lod_ratio, lod_index);
  const size_t original_index_count = original_indices.size();
  const size_t target_index_count = original_index_count * lod_scale / 3 * 3;
  const bool is_sparse = lod_scale <= 4 / std::sqrt(original_index_count);

  float lod_error = 0.f;

  result_indices.resize(original_index_count);

  uint32_t options =
      meshopt_SimplifyPrune | (is_sparse ? meshopt_SimplifySparse : 0);

  {
    ZoneScopedN("meshopt_simplify");

    result_indices.resize(meshopt_simplify(result_indices.data(),
        original_indices.data(),
        original_indices.size(),
        (float *)positions.data(),
        positions.size(),
        sizeof(Position),
        target_index_count,
        target_error,
        options,
        &lod_error));
  }

  {
    ZoneScopedN("meshopt_optimizeVertexCache");

    meshopt_optimizeVertexCache(result_indices.data(),
        result_indices.data(),
        result_indices.size(),
        positions.size());
  }

  {
    ZoneScopedN("meshopt_generateShadowIndexBufferMulti");

    result_shadow_indices.resize(result_indices.size());
    meshopt_generateShadowIndexBufferMulti(result_shadow_indices.data(),
        result_indices.data(),
        result_indices.size(),
        positions.size(),
        streams.data(),
        streams.size());
  }

  {
    ZoneScopedN("meshopt_optimizeVertexCache");

    meshopt_optimizeVertexCache(result_shadow_indices.data(),
        result_shadow_indices.data(),
        result_shadow_indices.size(),
        positions.size());
  }

  size_t original_index_array_size = index_array.size();
  size_t result_indices_size = result_indices.size();
  size_t result_shadow_indices_size = result_shadow_indices.size();

  IndexSpan result_indices_span;
  IndexSpan result_shadow_indices_span;

  static std::mutex index_array_mutex;
  {
    ZoneScopedN("Append LOD indices");

    std::lock_guard l(index_array_mutex);
    index_array.reserve(index_array.size() + result_indices.size() +
                        result_shadow_indices.size());
    index_array.append_range(std::move(result_indices));
    index_array.append_range(std::move(result_shadow_indices));

    result_indices_span = IndexSpan(
        index_array.data() + original_index_array_size, result_indices_size);
    result_shadow_indices_span = IndexSpan(
        index_array.data() + original_index_array_size + result_indices_size,
        result_shadow_indices_size);
  }

  return {result_indices_span, result_shadow_indices_span};
}
} // namespace

/**
 * Optimize mesh topology and build multiple LODs suitable for real-time
 * rendering.
 */
Mesh optimize(Mesh mesh)
{
  ZoneScoped;
  if (mesh.attributes.empty()) {
    MR_WARNING("Mesh has no attributes, but they are considered by `optimize` "
               "function."
               " Consider adding attribute-less path in optimize");
    mesh.attributes.resize(mesh.positions.size());
  }

  Mesh result;
  result.transforms = std::move(mesh.transforms);
  result.name = std::move(mesh.name);
  result.aabb = mesh.aabb;
  result.material = mesh.material;

  std::array streams = {
      meshopt_Stream {mesh.positions.data(),  sizeof(Position),         sizeof(Position)        },
      meshopt_Stream {mesh.attributes.data(), sizeof(VertexAttributes), sizeof(VertexAttributes)},
  };

  auto [count, ratio] =
      determine_lod_count_and_ratio(mesh.positions, mesh.lods[0].indices);
  result.indices.reserve(2 * mesh.indices.size() * (count + 1));
  result.lods.resize(count + 1);

  // improve vertex locality
  result.indices.resize(mesh.indices.size());
  meshopt_optimizeVertexCache(result.indices.data(),
      mesh.indices.data(),
      mesh.indices.size(),
      mesh.positions.size());

  // optimize overdraw
  meshopt_optimizeOverdraw(mesh.indices.data(),
      result.indices.data(),
      mesh.indices.size(),
      (float *)mesh.positions.data(),
      mesh.positions.size(),
      sizeof(Position),
      1.05f);

  IndexArray remap;
  remap.resize(mesh.indices.size());
  size_t vertex_count = meshopt_generateVertexRemapMulti(remap.data(),
      mesh.indices.data(),
      mesh.indices.size(),
      mesh.positions.size(),
      streams.data(),
      streams.size());

  result.indices.resize(mesh.indices.size());
  result.positions.resize(vertex_count);
  result.attributes.resize(vertex_count);

  meshopt_remapIndexBuffer(result.indices.data(),
      mesh.indices.data(),
      mesh.indices.size(),
      remap.data());
  meshopt_remapVertexBuffer(result.positions.data(),
      mesh.positions.data(),
      mesh.positions.size(),
      sizeof(Position),
      remap.data());
  meshopt_remapVertexBuffer(result.attributes.data(),
      mesh.attributes.data(),
      mesh.positions.size(),
      sizeof(VertexAttributes),
      remap.data());

  meshopt_optimizeVertexFetchRemap(remap.data(),
      result.indices.data(),
      result.indices.size(),
      result.positions.size());

  // NOTE: we run remap functions the second time as recommended by docs
  meshopt_remapIndexBuffer(result.indices.data(),
      result.indices.data(),
      result.indices.size(),
      remap.data());
  meshopt_remapVertexBuffer(result.positions.data(),
      result.positions.data(),
      result.positions.size(),
      sizeof(Position),
      remap.data());
  meshopt_remapVertexBuffer(result.attributes.data(),
      result.attributes.data(),
      result.attributes.size(),
      sizeof(VertexAttributes),
      remap.data());

  streams = {
    meshopt_Stream {result.positions.data(),  sizeof(Position),         sizeof(Position)        },
    meshopt_Stream {result.attributes.data(), sizeof(VertexAttributes), sizeof(VertexAttributes)},
  };

  result.lods[0].indices =
      IndexSpan(result.indices.data(), result.indices.size());
  result.lods[0].shadow_indices =
      IndexSpan(result.indices.data() + result.lods[0].indices.size(),
          result.lods[0].indices.size());
  result.indices.resize(result.indices.size() + result.indices.size());
  meshopt_generateShadowIndexBufferMulti(result.lods[0].shadow_indices.data(),
      result.lods[0].indices.data(),
      result.lods[0].indices.size(),
      result.positions.size(),
      streams.data(),
      streams.size());

  // LOD generation
  tbb::parallel_for<int>(1, count + 1, [&result, &streams, &ratio](int i) {
    std::tie(result.lods[i].indices, result.lods[i].shadow_indices) =
        generate_lod(result.positions,
            result.lods[0].indices,
            result.indices,
            streams,
            ratio,
            i);
  });

  for (int i = result.lods.size() - 1; i > 0; i--) {
    const auto &indices = result.lods[i];
    if (indices.indices.size() == 0) {
      result.lods.erase(result.lods.begin() + i);
    }
  }

  return result;
}

void add_optimizer_nodes(FlowGraph &graph, const Options &options)
{
  graph.meshes_optimize =
      std::make_unique<tbb::flow::function_node<fastgltf::Asset *>>(graph.graph,
          tbb::flow::unlimited,
          [&graph, &options](fastgltf::Asset *asset) {
            if (asset != nullptr && (options & Options::OptimizeMeshes)) {
              tbb::parallel_for_each(graph.model->meshes,
                  [](Mesh &mesh) { mesh = mr::optimize(std::move(mesh)); });
            }
          });
  tbb::flow::make_edge(*graph.meshes_load, *graph.meshes_optimize);
}

} // namespace importer
} // namespace mr
