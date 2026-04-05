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

  constexpr int maxlods = 5;
  constexpr int mintriangles = 12;

  size_t triangle_count = indices.size() / 3;
  if (triangle_count == 0) {
    return {0, 0};
  }
  float lod_scale = std::pow((float)mintriangles / triangle_count, 1.f / maxlods);
  size_t lod_count = std::ceil(std::log((float)mintriangles / triangle_count) / std::log(lod_scale));

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

[[nodiscard]] static std::pair<IndexSpan, IndexSpan> generate_lod(const PositionArray &positions,
    const VertexAttributesArray &attributes,
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
  const bool is_permissive = lod_scale <= 6 / std::sqrt(original_index_count);
  const bool is_sparse = lod_scale <= 4 / std::sqrt(original_index_count);
  const bool use_attributes = !is_sparse && !attributes.empty();

  float lod_error = 0.f;

  result_indices.resize(original_index_count);

  uint32_t options = meshopt_SimplifyPrune
    | (is_permissive ? meshopt_SimplifyPermissive : 0)
    | (is_sparse ? meshopt_SimplifySparse : 0)
  ;

  auto attribute_weights = attributes.weights();

  if (use_attributes) {
    ZoneScopedN("meshopt_simplifyWithAttributes");
    result_indices.resize(meshopt_simplifyWithAttributes(result_indices.data(),
          original_indices.data(),
          original_indices.size(),
          (float *)positions.data(),
          positions.size(),
          sizeof(Position),
          (float *)attributes.data(),
          sizeof(VertexAttributes),
          attribute_weights.data(),
          attribute_weights.size(),
          nullptr,
          target_index_count,
          target_error,
          options,
          &lod_error));
  }
  else {
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

    meshopt_optimizeVertexCache(
        result_indices.data(), result_indices.data(), result_indices.size(), positions.size());
  }

  result_shadow_indices.resize(result_indices.size());
  if (use_attributes) {
    ZoneScopedN("meshopt_generateShadowIndexBufferMulti");
    meshopt_generateShadowIndexBufferMulti(result_shadow_indices.data(),
        result_indices.data(),
        result_indices.size(),
        positions.size(),
        streams.data(),
        streams.size());
  }
  else {
    ZoneScopedN("meshopt_generateShadowIndexBuffer");
    meshopt_generateShadowIndexBuffer(result_shadow_indices.data(),
        result_indices.data(),
        result_indices.size(),
        positions.data(),
        positions.size(),
        sizeof(Position),
        sizeof(Position));
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

    result_indices_span =
        IndexSpan(index_array.data() + original_index_array_size, result_indices_size);
    result_shadow_indices_span =
        IndexSpan(index_array.data() + original_index_array_size + result_indices_size,
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

  if (indices.size() < 3) {
    return {};
  }

  size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, min_triangles);

  // clang-format off
  if (max_meshlets == 0) {
    MR_ERROR("Couldn't generate meshlets for a mesh with {} positions and {} indices", positions.size(), indices.size());
    return {};
  }

  MeshletArray meshlet_array;
  meshlet_array.meshlets.resize(max_meshlets);
  meshlet_array.meshlet_vertices.resize(indices.size());
  meshlet_array.meshlet_triangles.resize(indices.size() + max_meshlets * 3);

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

  if (meshlet_count == 0) {
    MR_ERROR("Couldn't generate meshlets for a mesh with {} positions and {} indices", positions.size(), indices.size());
    return {};
  }

  meshlet_array.meshlets.resize(meshlet_count);

  size_t meshlet_vertices_count =
      meshlet_array.meshlets.back().vertex_offset + meshlet_array.meshlets.back().vertex_count;
  size_t meshlet_triangle_count = meshlet_array.meshlets.back().triangle_offset +
                                  meshlet_array.meshlets.back().triangle_count * 3;

  meshlet_array.meshlet_vertices.resize(meshlet_vertices_count);
  meshlet_array.meshlet_triangles.resize(meshlet_triangle_count);

  for (size_t i = 0; i < meshlet_vertices_count; ++i) {
    if (meshlet_array.meshlet_vertices[i] >= positions.size()) {
      MR_ERROR("meshlet vertex index out of range: {} >= {} (positions)", meshlet_array.meshlet_vertices[i],
          positions.size());
      return {};
    }
  }

  {
    ZoneScopedN("meshopt_optimizeMeshlet");

    for (auto &meshlet : meshlet_array.meshlets) {
      meshopt_optimizeMeshlet(meshlet_array.meshlet_vertices.data() + meshlet.vertex_offset,
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
      meshopt_Bounds bounds =
          meshopt_computeMeshletBounds(&meshlet_array.meshlet_vertices[m.vertex_offset],
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
          bounds.cone_apex[0],
          bounds.cone_apex[1],
          bounds.cone_apex[2]
      };
      meshlet_bounds.cones[i].axis = {
          bounds.cone_axis[0],
          bounds.cone_axis[1],
          bounds.cone_axis[2]
      };
      meshlet_bounds.cones[i].cutoff = bounds.cone_cutoff;
    }
  }
  // clang-format on

  return {meshlet_array, meshlet_bounds};
}

void generate_lod_set(Mesh &result, std::span<meshopt_Stream> streams, int lodcount, float lodratio)
{
  tbb::parallel_for<int>(1, lodcount + 1, [&result, &streams, &lodratio](int i) {
    std::tie(result.lods[i].indices, result.lods[i].shadow_indices) = generate_lod(result.positions,
        result.attributes,
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

  if (mesh.indices.empty()) {
    return mesh;
  }
  if (mesh.lods.empty() || mesh.lods[0].indices.size() != mesh.indices.size()) {
    mesh.lods.clear();
    mesh.lods.emplace_back(IndexSpan(mesh.indices.data(), mesh.indices.size()), IndexSpan());
  }

  Mesh result;
  result.transforms = std::move(mesh.transforms);
  result.name = std::move(mesh.name);
  result.aabb = mesh.aabb;
  result.material = mesh.material;

  auto [count, ratio] = determine_lod_count_and_ratio(mesh.positions, mesh.lods[0].indices);
  result.indices.reserve(2 * mesh.indices.size() * (count + 1));
  result.lods.resize(count + 1);

  // improve vertex locality
  {
    ZoneScopedN("meshopt_optimizeVertexCache");
    result.indices.resize(mesh.indices.size());
    meshopt_optimizeVertexCache(
        result.indices.data(), mesh.indices.data(), mesh.indices.size(), mesh.positions.size());
  }

  // optimize overdraw
  {
    ZoneScopedN("meshopt_optimizeOverdraw");
    meshopt_optimizeOverdraw(mesh.indices.data(),
        result.indices.data(),
        mesh.indices.size(),
        (float *)mesh.positions.data(),
        mesh.positions.size(),
        sizeof(Position),
        1.05f);
  }

  size_t vertex_count = 0;
  IndexArray remap;
  remap.resize(mesh.indices.size());
  if (!mesh.attributes.empty()) {
    ZoneScopedN("meshopt_generateVertexRemapMulti");

    std::array streams = {
        meshopt_Stream{ mesh.positions.data(),         sizeof(Position),         sizeof(Position)},
        meshopt_Stream{mesh.attributes.data(), sizeof(VertexAttributes), sizeof(VertexAttributes)},
    };

    vertex_count = meshopt_generateVertexRemapMulti(remap.data(),
        mesh.indices.data(),
        mesh.indices.size(),
        mesh.positions.size(),
        streams.data(),
        streams.size());
  }
  else {
    ZoneScopedN("meshopt_generateVertexRemap");
    vertex_count = meshopt_generateVertexRemap(remap.data(),
        mesh.indices.data(),
        mesh.indices.size(),
        mesh.positions.data(),
        mesh.positions.size(),
        sizeof(Position));
  }

  result.indices.resize(mesh.indices.size());
  result.positions.resize(vertex_count);

  if (!mesh.attributes.empty()) {
    result.attributes.resize(vertex_count);
  }

  {
    ZoneScopedN("Remap Buffers");
    meshopt_remapIndexBuffer(
        result.indices.data(), mesh.indices.data(), mesh.indices.size(), remap.data());
    meshopt_remapVertexBuffer(result.positions.data(),
        mesh.positions.data(),
        mesh.positions.size(),
        sizeof(Position),
        remap.data());
    if (!mesh.attributes.empty()) {
      meshopt_remapVertexBuffer(result.attributes.data(),
          mesh.attributes.data(),
          mesh.attributes.size(),
          sizeof(VertexAttributes),
          remap.data());
    }
  }

  {
    ZoneScopedN("meshopt_optimizeVertexFetchRemap");
    meshopt_optimizeVertexFetchRemap(
        remap.data(), result.indices.data(), result.indices.size(), result.positions.size());
  }

  // Triangle indices occupy the first n entries; shadow indices follow. Resize first so
  // [data()+n, data()+2n) is valid storage—assigning shadow_indices before resize wrote
  // past size() (UB) and resize could invalidate spans if capacity was too small.
  size_t const ntri_idx = result.indices.size();
  result.indices.resize(ntri_idx * 2u);
  result.lods[0].indices = IndexSpan(result.indices.data(), ntri_idx);
  result.lods[0].shadow_indices = IndexSpan(result.indices.data() + ntri_idx, ntri_idx);

  if (!mesh.attributes.empty()) {
    ZoneScopedN("meshopt_generateShadowIndexBufferMulti");

    std::array streams = {
        meshopt_Stream{ result.positions.data(),         sizeof(Position),         sizeof(Position)},
        meshopt_Stream{result.attributes.data(), sizeof(VertexAttributes), sizeof(VertexAttributes)},
    };

    meshopt_generateShadowIndexBufferMulti(result.lods[0].shadow_indices.data(),
        result.lods[0].indices.data(),
        result.lods[0].indices.size(),
        result.positions.size(),
        streams.data(),
        streams.size());
  }
  else {
    ZoneScopedN("meshopt_generateShadowIndexBuffer");
    meshopt_generateShadowIndexBuffer(result.lods[0].shadow_indices.data(),
        result.lods[0].indices.data(),
        result.lods[0].indices.size(),
        result.positions.data(),
        result.positions.size(),
        sizeof(Position),
        sizeof(Position));
  }

  return result;
}

} // namespace

/*
 * LOAD -> [SPLIT] --> [OPT₁ -> LOD₁ -> MESHLETS₁] --> JOIN -> NEXT
 *                  \-> [OPT₂ -> LOD₂ -> MESHLETS₂]---/
 *                   \-> [OPT₃ -> LOD₃ -> MESHLETS₃]-/
 *                  ... (Each mesh independently)
 */
void add_optimizer_nodes(FlowGraph &graph, const Options &options)
{
  graph.split_meshes =
      std::make_unique<tbb::flow::function_node<void *, std::vector<size_t>>>(
          graph.graph, 1, [&graph](void *token) -> std::vector<size_t> {
            if (token == nullptr || !graph.model)
              return {};

            std::vector<size_t> indices(graph.model->meshes.size());
            std::iota(indices.begin(), indices.end(), 0);
            return indices;
          });

  graph.mesh_index_broadcaster = std::make_unique<tbb::flow::broadcast_node<size_t>>(graph.graph);

  graph.mesh_processor = std::make_unique<tbb::flow::function_node<size_t, size_t>>(
      graph.graph, tbb::flow::unlimited, [&graph, &options](size_t mesh_idx) -> size_t {
        if (!graph.model) {
          return mesh_idx;
        }

        Mesh &mesh = graph.model->meshes[mesh_idx];

        if (options & Options::OptimizeMeshes) {
          mesh = optimize_data_layout(std::move(mesh));
        }

        // clang-format off
        if (options & Options::GenerateDiscreteLODs && !mesh.lods.empty()
            && mesh.lods[0].indices.size() >= 3) {
          std::array streams = {
              meshopt_Stream {mesh.positions.data(),  sizeof(Position),         sizeof(Position)        },
              meshopt_Stream {mesh.attributes.data(), sizeof(VertexAttributes), sizeof(VertexAttributes)},
          };
          auto [count, ratio] = determine_lod_count_and_ratio(mesh.positions, mesh.lods[0].indices);
          generate_lod_set(mesh, streams, count, ratio);
        }

        if (options & Options::GenerateMeshlets) {
          tbb::parallel_for<size_t>(0, mesh.lods.size(), [&mesh](size_t i) {
            if (mesh.lods[i].indices.size() < 3) {
              return;
            }
            std::tie(mesh.lods[i].meshlet_array, mesh.lods[i].meshlet_bounds) =
                generate_meshlets(mesh.positions, mesh.lods[i].indices);
          });
        }
        // clang-format on

        return mesh_idx;
      });

  // This collects all mesh indices and passes the pipeline token forward
  graph.meshes_join = std::make_unique<
      tbb::flow::join_node<std::tuple<size_t, void *>, tbb::flow::queueing>>(
      graph.graph);

  graph.continue_after_meshes = std::make_unique<
      tbb::flow::function_node<std::tuple<size_t, void *>, void *>>(
      graph.graph,
      1,
      [&graph](const std::tuple<size_t, void *> &input) -> void * {
        return std::get<1>(input);
      });

  // creates separate messages for each mesh to enable independent processing
  graph.vector_splitter =
      std::make_unique<tbb::flow::function_node<std::vector<size_t>, tbb::flow::continue_msg>>(
          graph.graph, 1, [&graph](const std::vector<size_t> &indices) -> tbb::flow::continue_msg {
            for (size_t idx : indices) {
              graph.mesh_index_broadcaster->try_put(idx);
            }
            return {};
          });

  graph.asset_replicator =
      std::make_unique<tbb::flow::function_node<void *, void *>>(
          graph.graph, tbb::flow::unlimited, [&graph](void *token) -> void * {
            if (graph.model) {
              for (size_t i = 0; i < graph.model->meshes.size(); ++i) {
              }
            }
            return token;
          });

  // clang-format off
  tbb::flow::make_edge(*graph.meshes_load, *graph.split_meshes);
  tbb::flow::make_edge(*graph.split_meshes, *graph.vector_splitter);
  tbb::flow::make_edge(*graph.mesh_index_broadcaster, *graph.mesh_processor);
  tbb::flow::make_edge(*graph.mesh_processor, tbb::flow::input_port<0>(*graph.meshes_join));
  tbb::flow::make_edge(*graph.meshes_load, *graph.asset_replicator);
  tbb::flow::make_edge(*graph.asset_replicator, tbb::flow::input_port<1>(*graph.meshes_join));
  tbb::flow::make_edge(*graph.meshes_join, *graph.continue_after_meshes);
  // clang-format on
}

} // namespace importer
} // namespace mr
