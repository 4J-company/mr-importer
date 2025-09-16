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
