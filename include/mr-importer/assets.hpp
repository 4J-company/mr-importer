#pragma once

#include "def.hpp"

namespace mr {
inline namespace importer {
  using Position = glm::vec3;
  using Index = std::uint32_t;
  using Transform = glm::mat4x4;
  using Color = glm::vec4;
  struct VertexAttributes {
    Color color;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
  };

  // mesh-related data
  struct PositionArray : std::vector<Position> {
    using std::vector<Position>::vector;
    using std::vector<Position>::operator=;
  };

  struct IndexArray : std::vector<Index> {
    using std::vector<Index>::vector;
    using std::vector<Index>::operator=;
  };

  struct VertexAttributesArray : std::vector<VertexAttributes> {
    using std::vector<VertexAttributes>::vector;
    using std::vector<VertexAttributes>::operator=;
  };

  struct Mesh {
    struct LOD {
	    IndexArray indices;
	    IndexArray shadow_indices;
    };

    PositionArray positions;
    VertexAttributesArray attributes;
    std::vector<LOD> lods;
    std::vector<Transform> transforms;
    std::string name;
  };

  // material-related data
  struct ImageData {
    // unique ptr because memory is allocated by stb and passed to us
    std::unique_ptr<Color[]> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
  };
  struct SamplerData {
    // TODO: copy fastgltf::Sampler
  };
  struct TextureData {
    ImageData image;
    SamplerData sampler;
  };
  struct MaterialData {
    std::vector<TextureData> textures;

    Color base_color_factor;
    Color emissive_color;
    float emissive_strength;
    float normal_map_intensity;

    float roughness_factor;
    float metallic_factor;
  };

  // TODO: animation-related data

  struct Camera {
  };

  struct DirectionalLight {
    Color color;
    float intensity;
  };
  struct SpotLight {
    Color color;
    float intensity;
  };
  struct PointLight {
    Color color;
    float intensity;
    float radius;
  };

  struct Asset {
    std::vector<Mesh> meshes;
    std::vector<MaterialData> materials;

    Asset() = default;
    Asset(const std::filesystem::path &path);
  };
}
}
