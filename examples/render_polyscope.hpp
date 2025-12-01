#include "mr-importer/assets.hpp"
#include <polyscope/render/materials.h>

#include <glm/glm.hpp>

#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

#include <cstdint>
#include <vector>
#include <array>

inline std::string remove_hashtags(std::string_view format_str) {
  std::string result;
  result.reserve(format_str.size()); // Optimize by pre-allocating memory
  for (char c : format_str) {
    if (c != '#') {
      result += c;
    }
  }
  return result;
}

template <typename T>
inline std::vector<std::array<T, 3>> convertToArrayOfTriples(const std::span<T> &input) {
  const uint32_t newSize = input.size() / 3;
  std::vector<std::array<T, 3>> result;
  result.reserve(newSize);

  for (uint32_t i = 0; i < input.size(); i += 3) {
    result.push_back({input[i], input[i+1], input[i+2]});
  }

  return result;
}

struct TemporaryMesh {
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<uint32_t, 3>> indices;
};

TemporaryMesh extractMeshlet(const mr::Mesh& mesh, const mr::Mesh::LOD& lod, size_t meshlet_index) {
    TemporaryMesh result;
    
    const auto& meshlet = lod.meshlet_array.meshlets[meshlet_index];
    const auto& meshlet_vertices = lod.meshlet_array.meshlet_vertices;
    const auto& meshlet_triangles = lod.meshlet_array.meshlet_triangles;
    
    result.positions.reserve(meshlet.vertex_count);
    for (uint32_t i = 0; i < meshlet.vertex_count; ++i) {
        uint32_t vertex_index = meshlet_vertices[meshlet.vertex_offset + i];
        result.positions.push_back(mesh.positions[vertex_index]);
    }
    
    result.indices.reserve(meshlet.triangle_count);
    for (uint32_t i = 0; i < meshlet.triangle_count; i++) {
        uint32_t triangle_index = meshlet.triangle_offset + i * 3;

        uint8_t v0 = meshlet_triangles[triangle_index + 0];
        uint8_t v1 = meshlet_triangles[triangle_index + 1];
        uint8_t v2 = meshlet_triangles[triangle_index + 2];

        result.indices.push_back({v0, v1, v2});
    }
    
    return result;
}

inline void render_meshlets(const std::vector<mr::Mesh> &meshes) {
  polyscope::init();

  // Disable ground
  polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::None;
  // Set camera to FPS-like
  polyscope::view::setNavigateStyle(polyscope::NavigateStyle::FirstPerson);

  for (int i = 0; i < meshes.size(); i++) {
    auto& mesh = meshes[i];

    int lodnumber = 0;
    auto& lod = mesh.lods.size() - 1 < lodnumber ? mesh.lods.back() : mesh.lods[lodnumber];
    auto& pos = mesh.positions;

    for (int k = 0; k < mesh.transforms.size(); k++) {
      auto mrt = mesh.transforms[k];
      glm::mat4 t = {
        mrt[0][0], mrt[1][0], mrt[2][0], mrt[3][0],
        mrt[0][1], mrt[1][1], mrt[2][1], mrt[3][1],
        mrt[0][2], mrt[1][2], mrt[2][2], mrt[3][2],
        mrt[0][3], mrt[1][3], mrt[2][3], mrt[3][3],
      };

      for (int j = 0; j < lod.meshlet_array.meshlets.size(); j++) {
        auto& meshlet = lod.meshlet_array.meshlets[j];

        auto fmt = std::format("Mesh {}{}; Instance {}; Meshlet {}", mesh.name, i, k, j);

        auto x = extractMeshlet(mesh, lod, j);
        auto* meshptr = polyscope::registerSurfaceMesh(remove_hashtags(fmt), x.positions, x.indices);
        meshptr->setTransform(t);
        // meshptr->setMaterial("normal");
        meshptr->setEdgeWidth(1.0);  // Enable edge rendering by default
      }
    }
  }

  polyscope::show();
}

inline void render(const std::vector<mr::Mesh> &meshes) {
  polyscope::init();

  // Disable ground
  polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::None;
  // Set camera to FPS-like
  polyscope::view::setNavigateStyle(polyscope::NavigateStyle::FirstPerson);

  float xoffset = 0;
  for (int i = 0; i < meshes.size(); i++) {
    auto& mesh = meshes[i];

    int lodnumber = 0;
    auto& lod = mesh.lods.size() - 1 < lodnumber ? mesh.lods.back() : mesh.lods[lodnumber];
    auto& pos = mesh.positions;
    auto ind = convertToArrayOfTriples(lod.indices);

    for (int k = 0; k < mesh.transforms.size(); k++) {
      auto fmt = std::format("Mesh {}{}; Instance {}", mesh.name, i, k);
      auto* meshptr = polyscope::registerSurfaceMesh(remove_hashtags(fmt), pos, ind);
      auto mrt = mesh.transforms[k];
      glm::mat4 t = {
        mrt[0][0], mrt[1][0], mrt[2][0], mrt[3][0],
        mrt[0][1], mrt[1][1], mrt[2][1], mrt[3][1],
        mrt[0][2], mrt[1][2], mrt[2][2], mrt[3][2],
        mrt[0][3], mrt[1][3], mrt[2][3], mrt[3][3],
      };
      meshptr->setTransform(t);
      // meshptr->setMaterial("normal");
      meshptr->setEdgeWidth(1.0);  // Enable edge rendering by default
    }
  }

  polyscope::show();
}
