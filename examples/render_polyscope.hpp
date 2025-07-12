#include "mr-importer/assets.hpp"
#include <polyscope/render/materials.h>

#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

#include <glm/glm.hpp>

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
inline std::vector<std::array<T, 3>> convertToArrayOfTriples(const std::vector<T>& input) {
  const uint32_t newSize = input.size() / 3;
  std::vector<std::array<T, 3>> result;
  result.reserve(newSize);

  for (uint32_t i = 0; i < input.size(); i += 3) {
    result.push_back({input[i], input[i+1], input[i+2]});
  }

  return result;
}

inline void render(std::vector<glm::vec3> positions, std::vector<uint32_t> indices) {
  polyscope::init();
  auto mesh = polyscope::registerSurfaceMesh("my mesh", positions, convertToArrayOfTriples(indices));
  polyscope::show();
}

inline void render(std::vector<mr::Mesh> meshes) {
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
      meshptr->setTransform(mesh.transforms[k]);
      // meshptr->setMaterial("normal");
      meshptr->setEdgeWidth(1.0);  // Enable edge rendering by default
    }
  }

  polyscope::show();
}
