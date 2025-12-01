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

  float lod_scale = 0.1;
  size_t lod_count = lod_count =
      std::ceil(std::log(3.f * 47 / indices.size()) / std::log(lod_scale));

  // we want any mesh to have at least 47 triangles
  if (lod_count < 1) {
    return {0, 0};
  }

  if (lod_count > maxlods) {
    lod_scale = std::pow(lod_scale, lod_count / (float)maxlods);
    lod_count = maxlods;
  }

  return {lod_count, lod_scale};
}

[[nodiscard]] static std::pair<IndexSpan, IndexSpan> generate_lod(
    const PositionArray &positions,
    const IndexSpan &original_indices,
    IndexArray &index_array,
    const std::span<meshopt_Stream> &streams,
    float lod_ratio,
    int lod_index)
{
  ZoneScoped;
  ZoneValue(lod_index);
  ZoneValue(lod_ratio);

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

  IndexSpan result_indices_span;
  IndexSpan result_shadow_indices_span;

  static std::mutex index_array_mutex;
  {
    ZoneScopedN("Append LOD indices");

    std::lock_guard l(index_array_mutex);

    size_t original_index_array_size = index_array.size();
    size_t result_indices_size = result_indices.size();
    size_t result_shadow_indices_size = result_shadow_indices.size();

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

[[nodiscard]] std::pair<MeshletArray, MeshletBoundsArray> generate_meshlets(
    const PositionArray &positions, IndexSpan indices)
{
  ZoneScoped;
  ZoneValue(positions.size());
  ZoneValue(indices.size());

  constexpr float cone_weight = 0.25;
  constexpr float split_factor = 2.0;
  constexpr size_t max_vertices = 96;
  constexpr size_t min_triangles = 96;
  constexpr size_t max_triangles = 124; // up to 126, but divisible by 4

  size_t max_meshlets =
      meshopt_buildMeshletsBound(indices.size(), max_vertices, min_triangles);

  if (max_meshlets == 0) {
    MR_ERROR("Couldn't generate meshlets for a mesh with {} positions and {} indices", positions.size(), indices.size());
    return {};
  }

  MeshletArray meshlet_array;
  meshlet_array.meshlets.resize(max_meshlets);
  meshlet_array.meshlet_vertices.resize(indices.size());
  meshlet_array.meshlet_triangles.resize(indices.size() + max_meshlets * 3);

  // clang-format off
  static_assert(sizeof(meshopt_Meshlet) == sizeof(Meshlet) &&
      alignof(meshopt_Meshlet) == alignof(Meshlet) &&
      offsetof(meshopt_Meshlet, vertex_offset) == offsetof(Meshlet, vertex_offset) &&
      offsetof(meshopt_Meshlet, vertex_offset) == offsetof(Meshlet, vertex_offset) &&
      offsetof(meshopt_Meshlet, vertex_offset) == offsetof(Meshlet, vertex_offset) &&
      offsetof(meshopt_Meshlet, vertex_offset) == offsetof(Meshlet, vertex_offset) &&
      "Line below relies on the fact that meshopt_Meshlet and mr::Meshlet share the same internal structure.\n"
      "Change mr::Meshlet data layout (or variable names/types) back or perform an explicit transition");

  size_t meshlet_count = 0;
  {
    ZoneScopedN("meshopt_buildMeshletsFlex");

    meshlet_count = meshopt_buildMeshletsFlex((meshopt_Meshlet *)meshlet_array.meshlets.data(),
        meshlet_array.meshlet_vertices.data(), meshlet_array.meshlet_triangles.data(),
        indices.data(), indices.size(),
        (float *)positions.data(), positions.size(), sizeof(Position),
        max_vertices, min_triangles, max_triangles, cone_weight, split_factor);
  }
  // clang-format on

  if (meshlet_count == 0) {
    MR_ERROR("Couldn't generate meshlets for a mesh with {} positions and {} indices", positions.size(), indices.size());
    return {};
  }

  meshlet_array.meshlets.resize(meshlet_count);

  size_t meshlet_vertices_count = meshlet_array.meshlets.back().vertex_offset +
                                  meshlet_array.meshlets.back().vertex_count;
  size_t meshlet_triangle_count =
      meshlet_array.meshlets.back().triangle_offset +
      meshlet_array.meshlets.back().triangle_count * 3;

  meshlet_array.meshlet_vertices.resize(meshlet_vertices_count);
  meshlet_array.meshlet_triangles.resize(meshlet_triangle_count);

  {
    ZoneScopedN("meshopt_optimizeMeshlet");

    for (auto &meshlet : meshlet_array.meshlets) {
      meshopt_optimizeMeshlet(
          meshlet_array.meshlet_vertices.data() + meshlet.vertex_offset,
          meshlet_array.meshlet_triangles.data() + meshlet.triangle_offset,
          meshlet.triangle_count,
          meshlet.vertex_count);
    }
  }

  MeshletBoundsArray meshlet_bounds;
  meshlet_bounds.bounding_spheres.resize(meshlet_array.meshlets.size());
  meshlet_bounds.packed_cones.resize(meshlet_array.meshlets.size());
  meshlet_bounds.cones.resize(meshlet_array.meshlets.size());

  {
    ZoneScopedN("meshopt_computeMeshletBounds");

    for (int i = 0; i < meshlet_array.meshlets.size(); i++) {
      const auto &m = meshlet_array.meshlets[i];
      meshopt_Bounds bounds = meshopt_computeMeshletBounds(
          &meshlet_array.meshlet_vertices[m.vertex_offset],
          &meshlet_array.meshlet_triangles[m.triangle_offset],
          m.triangle_count,
          (float *)positions.data(),
          positions.size(),
          sizeof(Position));

      meshlet_bounds.bounding_spheres[i].data = {
          bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius};

      meshlet_bounds.packed_cones[i].axis[0] = bounds.cone_axis_s8[0];
      meshlet_bounds.packed_cones[i].axis[1] = bounds.cone_axis_s8[1];
      meshlet_bounds.packed_cones[i].axis[2] = bounds.cone_axis_s8[2];
      meshlet_bounds.packed_cones[i].cutoff = bounds.cone_cutoff_s8;

      meshlet_bounds.cones[i].apex = {
          bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2]};
      meshlet_bounds.cones[i].axis = {
          bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]};
      meshlet_bounds.cones[i].cutoff = bounds.cone_cutoff;
    }
  }

  return {meshlet_array, meshlet_bounds};
}

void generate_lod_set(Mesh &result,
    std::span<meshopt_Stream> streams,
    int lodcount,
    float lodratio)
{
  // LOD generation
  tbb::parallel_for<int>(
      1, lodcount + 1, [&result, &streams, &lodratio](int i) {
        std::tie(result.lods[i].indices, result.lods[i].shadow_indices) =
            generate_lod(result.positions,
                result.lods[0].indices,
                result.indices,
                streams,
                lodratio,
                i);
      });

  for (int i = result.lods.size() - 1; i > 0; i--) {
    const auto &indices = result.lods[i];
    if (indices.indices.size() == 0) {
      result.lods.erase(result.lods.begin() + i);
    }
  }
}

/**
 * Optimize mesh geometry data layout.
 */
Mesh optimize_data_layout(Mesh mesh)
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
      meshopt_Stream{ mesh.positions.data(),sizeof(Position),sizeof(Position)                  },
      meshopt_Stream{mesh.attributes.data(),
                     sizeof(VertexAttributes),
                     sizeof(VertexAttributes)},
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
      meshopt_Stream{
                     result.positions.data(),sizeof(Position),sizeof(Position)                  },
      meshopt_Stream{result.attributes.data(),
                     sizeof(VertexAttributes),
                     sizeof(VertexAttributes)},
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

  return result;
}

} // namespace

void add_optimizer_nodes(FlowGraph &graph, const Options &options)
{
  graph.meshes_optimize = std::make_unique<
      tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>>(
      graph.graph,
      tbb::flow::unlimited,
      [&graph, &options](fastgltf::Asset *asset) {
        if (asset != nullptr && (options & Options::OptimizeMeshes)) {
          tbb::parallel_for_each(graph.model->meshes, [](Mesh &mesh) {
            mesh = optimize_data_layout(std::move(mesh));
          });
        }
        return asset;
      });

  graph.mesh_lod_generate = std::make_unique<
      tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>>(
      graph.graph,
      tbb::flow::unlimited,
      [&graph, &options](fastgltf::Asset *asset) {
        // clang-format off
        if (asset != nullptr && (options & Options::GenerateDiscreteLODs)) {
          tbb::parallel_for_each(graph.model->meshes, [](Mesh &mesh) {
            std::array streams = {
              meshopt_Stream {mesh.positions.data(),  sizeof(Position),         sizeof(Position)},
              meshopt_Stream {mesh.attributes.data(), sizeof(VertexAttributes), sizeof(VertexAttributes)},
            };
            auto [count, ratio] = determine_lod_count_and_ratio(mesh.positions, mesh.lods[0].indices);
            generate_lod_set(mesh, streams, count, ratio);
          });
        }
        // clang-format on
        return asset;
      });

  graph.meshlet_generate = std::make_unique<
      tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>>(
      graph.graph,
      tbb::flow::unlimited,
      [&graph, &options](fastgltf::Asset *asset) {
        if (asset != nullptr && (options & Options::GenerateMeshlets)) {
          tbb::parallel_for_each(graph.model->meshes, [](Mesh &mesh) {
            tbb::parallel_for<size_t>(0, mesh.lods.size(), [&mesh](size_t i) {
                std::tie(mesh.lods[i].meshlet_array, mesh.lods[i].meshlet_bounds) = generate_meshlets(mesh.positions, mesh.lods[i].indices);
            });
          });
        }
        return asset;
      });

  tbb::flow::make_edge(*graph.meshes_load, *graph.meshes_optimize);
  tbb::flow::make_edge(*graph.meshes_optimize, *graph.mesh_lod_generate);
  tbb::flow::make_edge(*graph.mesh_lod_generate, *graph.meshlet_generate);
}

} // namespace importer
} // namespace mr
