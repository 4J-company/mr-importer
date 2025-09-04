#include "mr-importer/importer.hpp"

#include "pch.hpp"

namespace mr {
inline namespace importer {
  std::pair<size_t, float> determine_lod_count_and_ratio(const PositionArray &positions) {
    constexpr int maxlods = 3;

    float lod_scale = meshopt_simplifyScale((float*)positions.data(), positions.size(), sizeof(Position));
    size_t lod_count = 0;

    if (lod_scale > 1) {
        MR_WARNING("LOD Scale greater than 1. Needs investigation");
        lod_scale = 0.1;
    }

    // we want any mesh to have at least 47 triangles
    while (positions.size() * std::pow(lod_scale, lod_count) / 3 >= 47) {
      lod_count++;
    }

    if (lod_count > maxlods) {
      lod_scale = std::pow(lod_scale, lod_count / (float)maxlods);
      lod_count = maxlods;
    }

    return {lod_count, lod_scale};
  }

  std::pair<IndexArray, IndexArray> generate_lod(
    const PositionArray &positions,
    const IndexArray &original_indices,
    const std::span<meshopt_Stream> &streams,
    float lod_ratio,
    int lod_index)
  {
    static constexpr float target_error = 0.05f;

    IndexArray result_indices;
    IndexArray result_shadow_indices;

    const float lod_scale = std::pow(lod_ratio, lod_index);
    const size_t original_index_count = original_indices.size();
    const size_t target_index_count = original_index_count * lod_scale / 3 * 3;
    const bool is_sparse = lod_scale <= 4 / std::sqrt(original_index_count);

    float lod_error = 0.f;

    result_indices.resize(original_index_count);

    uint32_t options
      = meshopt_SimplifyPrune
      | (is_sparse ? meshopt_SimplifySparse : 0)
    ;

    result_indices.resize(
      meshopt_simplify(
        result_indices.data(),
        original_indices.data(), original_indices.size(),
        (float*)positions.data(), positions.size(), sizeof(Position),
        target_index_count, target_error,
        options, &lod_error)
    );

    MR_INFO("LOD {}:", lod_index);
    MR_INFO("\tTarget index count: {}", target_index_count);
    MR_INFO("\tIs sparse: {}", is_sparse);
    MR_INFO("\tError: {}", lod_error);

    meshopt_optimizeVertexCache(result_indices.data(), result_indices.data(), result_indices.size(), positions.size());

    result_shadow_indices.resize(result_indices.size());
    meshopt_generateShadowIndexBufferMulti(result_shadow_indices.data(),
                                           result_indices.data(), result_indices.size(),
                                           positions.size(),
                                           streams.data(), streams.size());
    meshopt_optimizeVertexCache(result_shadow_indices.data(), result_shadow_indices.data(), result_shadow_indices.size(), positions.size());

    return {result_indices, result_shadow_indices};
  }

  Mesh optimize(Mesh mesh) {
    if (mesh.attributes.empty()) {
      mesh.attributes.resize(mesh.positions.size());
    }

    Mesh result;
    result.transforms = mesh.transforms;
    result.name = mesh.name;

    std::array streams = {
      meshopt_Stream{mesh.positions.data(), sizeof(Position), sizeof(Position)},
      meshopt_Stream{mesh.attributes.data(), sizeof(VertexAttributes), sizeof(VertexAttributes)},
    };

    auto [count, ratio] = determine_lod_count_and_ratio(mesh.positions);
    result.lods.resize(count+1);

    // improve vertex locality
    meshopt_optimizeVertexCache(mesh.lods[0].indices.data(), mesh.lods[0].indices.data(), mesh.lods[0].indices.size(), mesh.positions.size());

    // optimize overdraw
    meshopt_optimizeOverdraw(mesh.lods[0].indices.data(), mesh.lods[0].indices.data(), mesh.lods[0].indices.size(),
                             (float*)mesh.positions.data(), mesh.positions.size(), sizeof(Position), 1.05f);

    IndexArray remap;
    remap.resize(mesh.lods[0].indices.size());
    size_t vertex_count = meshopt_generateVertexRemapMulti(
      remap.data(),
      mesh.lods[0].indices.data(),
      mesh.lods[0].indices.size(),
      mesh.positions.size(),
      streams.data(),
      streams.size());

    result.lods[0].indices.resize(mesh.lods[0].indices.size());
    result.positions.resize(vertex_count);
    result.attributes.resize(vertex_count);

    meshopt_remapIndexBuffer(result.lods[0].indices.data(), mesh.lods[0].indices.data(), mesh.lods[0].indices.size(), remap.data());
    meshopt_remapVertexBuffer(result.positions.data(), mesh.positions.data(), mesh.positions.size(), sizeof(Position), remap.data());
    meshopt_remapVertexBuffer(result.attributes.data(), mesh.attributes.data(), mesh.positions.size(), sizeof(VertexAttributes), remap.data());

    meshopt_optimizeVertexFetchRemap(
      remap.data(),
      result.lods[0].indices.data(), result.lods[0].indices.size(),
      result.positions.size());

    // NOTE: we run remap functions the second time as recommended by docs
    meshopt_remapIndexBuffer(result.lods[0].indices.data(), result.lods[0].indices.data(), result.lods[0].indices.size(), remap.data());
    meshopt_remapVertexBuffer(result.positions.data(), result.positions.data(), result.positions.size(), sizeof(Position), remap.data());
    meshopt_remapVertexBuffer(result.attributes.data(), result.attributes.data(), result.attributes.size(), sizeof(VertexAttributes), remap.data());

    result.lods[0].shadow_indices.resize(result.lods[0].indices.size());
    meshopt_generateShadowIndexBufferMulti(result.lods[0].shadow_indices.data(),
                                           result.lods[0].indices.data(), result.lods[0].indices.size(),
                                           result.positions.size(),
                                           streams.data(), streams.size());

    // LOD generation
    for (int i = 1; i < count+1; i++) {
      std::tie(result.lods[i].indices, result.lods[i].shadow_indices) =
        generate_lod(result.positions, result.lods[0].indices, streams, ratio, i);
    }

    return result;
  }
}
}
