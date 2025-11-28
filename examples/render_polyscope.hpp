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

inline void render_meshlets(const mr::Model &model, int lodnumber) {
  polyscope::init();

  // Disable ground
  polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::None;
  // Set camera to FPS-like
  polyscope::view::setNavigateStyle(polyscope::NavigateStyle::FirstPerson);

  for (int i = 0; i < model.meshes.size(); i++) {
    auto& mesh = model.meshes[i];

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

inline void render(const mr::Model &model, int lodnumber) {
  polyscope::init();

  // Disable ground
  polyscope::options::groundPlaneMode = polyscope::GroundPlaneMode::None;
  polyscope::options::enableVSync = false;
  polyscope::options::maxFPS = -1;

  // Set camera to FPS-like
  polyscope::view::setNavigateStyle(polyscope::NavigateStyle::FirstPerson);

  float xoffset = 0;
  for (int i = 0; i < model.meshes.size(); i++) {
    auto& mesh = model.meshes[i];

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

#if 0
      std::vector<glm::vec2> uvs;
      for (auto attr : mesh.attributes) {
        uvs.emplace_back(
            attr.texcoord.x(),
            attr.texcoord.y()
        );
      }
      auto *qParam = meshptr->addVertexParameterizationQuantity("UV", uvs);

      if (mesh.material == -1) {
        printf("Mesh has no material\n");
        continue;
      }

      int index = 0;
      auto &textures = model.materials[mesh.material].textures;
      while (index < textures.size() && textures[index].type != mr::importer::TextureType::BaseColor) {
        index++;
      }
      if (index == textures.size()) {
        printf("Mesh has no base color texture\n");
        continue;
      }

      auto &texture = textures[index];
      auto &image = texture.image;

      size_t pixels = image.mips[0].size() / image.bytes_per_pixel;
      std::vector<glm::vec3> colors;
      colors.resize(pixels);

      if (image.format == vk::Format::eR8G8B8Srgb) {
        std::memcpy(colors.data(), image.pixels.get(), image.mips[0].size());
      }
      else if (image.format == vk::Format::eR8G8B8A8Srgb) {
        for (int i = 0; i < pixels; i++) {
          colors[i] = {
            (unsigned char)image.mips[0][4 * i + 0] / 256.f,
            (unsigned char)image.mips[0][4 * i + 1] / 256.f,
            (unsigned char)image.mips[0][4 * i + 2] / 256.f,
          };
        }
      }
      else if (image.format == vk::Format::eB8G8R8A8Srgb) {
        for (int i = 0; i < pixels; i++) {
          colors[i] = {
            (unsigned char)image.mips[0][4 * i + 2] / 256.f,
            (unsigned char)image.mips[0][4 * i + 1] / 256.f,
            (unsigned char)image.mips[0][4 * i + 0] / 256.f,
          };
        }
      }
      else if (image.format == vk::Format::eB8G8R8Srgb) {
        for (int i = 0; i < pixels; i++) {
          colors[i] = {
            (unsigned char)image.mips[0][3 * i + 2] / 256.f,
            (unsigned char)image.mips[0][3 * i + 1] / 256.f,
            (unsigned char)image.mips[0][3 * i + 0] / 256.f,
          };
        }
      }
      else {
          printf("Bullshit format: %d %s\n", image.format, fmt.c_str());
          continue;
      }

      auto *qColor = meshptr->addTextureColorQuantity("tColor", *qParam, image.width, image.height, colors, polyscope::ImageOrigin::UpperLeft);
      qColor->setFilterMode(texture.sampler.mag == vk::Filter::eNearest ? polyscope::FilterMode::Nearest : polyscope::FilterMode::Linear);
      qColor->setEnabled(true);
#endif
    }
  }

#if 1
  // Get initial camera parameters for reference distance
  polyscope::CameraParameters params_old = polyscope::view::getCameraParametersForCurrentView();
  glm::vec3 initialPos = params_old.getPosition();
  float initialDistance = glm::length(initialPos);

  // Define distance range (from very close to very distant)
  std::vector<float> distances;
  distances.push_back(initialDistance / 2);
  for (int k = 0; k < 7; k++) {
    distances.push_back(initialDistance * std::pow(2.5, k));
  }

  for (int i = 0; i < 8; i++) {
    for (int j = -1; j < 2; j++) {
      for (float currentDistance : distances) {
        // Calculate angles in radians
        float horizontalAngle = glm::radians(45.0f * i);
        float verticalAngle = glm::radians(45.0f * j);

        printf("%f\n", currentDistance);

        // Calculate camera position on sphere
        float x = currentDistance * cos(verticalAngle) * sin(horizontalAngle);
        float y = currentDistance * sin(verticalAngle);
        float z = currentDistance * cos(verticalAngle) * cos(horizontalAngle);

        glm::vec3 cameraPos(x, y, z);
        glm::vec3 target(0.0f, 1.0f, 0.0f); // Look at origin
        glm::vec3 worldUp(0.0f, 1.0f, 0.0f); // Y-up world

        // Create view matrix using lookAt
        glm::mat4 viewMatrix = glm::lookAt(cameraPos + glm::vec3(0.f, 1.f, 0.f), target, worldUp);

        // Set the camera view matrix
        polyscope::view::setCameraViewMatrix(viewMatrix);

        polyscope::view::nearClipRatio = 0.01;
        polyscope::view::farClipRatio = 1000.0;

        // polyscope::draw(false);

        polyscope::screenshot();
      }
    }
  }
#else
  polyscope::show();
#endif
}
