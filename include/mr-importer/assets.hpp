#pragma once

#include "def.hpp"

namespace mr {
inline namespace importer {
  /**
   * \file assets.hpp
   * \brief Core data structures owned and returned by the importer.
   */
  
  /** \brief 3D position in object space. */
  using Position = glm::vec3;
  /** \brief Index into vertex arrays. */
  using Index = std::uint32_t;
  /** \brief Local-to-world transform matrix. */
  using Transform = glm::mat4x4;
  /** \brief RGBA color in linear space. */
  using Color = glm::vec4;
  /** \brief Per-vertex attributes used by the renderer. */
  struct VertexAttributes {
    Color color;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
  };
  
  // mesh-related data
  /** \brief Contiguous array of vertex positions. */
  struct PositionArray : std::vector<Position> {
    using std::vector<Position>::vector;
    using std::vector<Position>::operator=;
  };
  
  /** \brief Contiguous array of triangle indices. */
  struct IndexArray : std::vector<Index> {
    using std::vector<Index>::vector;
    using std::vector<Index>::operator=;
  };
  
  /** \brief Contiguous array of per-vertex attributes. */
  struct VertexAttributesArray : std::vector<VertexAttributes> {
    using std::vector<VertexAttributes>::vector;
    using std::vector<VertexAttributes>::operator=;
  };
  
  /** \brief Renderable mesh with positions, attributes and LODs. */
  struct Mesh {
    /** \brief One level-of-detail of mesh indices. */
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
  /** \brief Raw image data stored as linear RGBA float pixels. */
  struct ImageData {
    // unique ptr because memory is allocated by stb and passed to us
    std::unique_ptr<Color[]> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
  };
  /** \brief Texture sampler settings placeholder. */
  struct SamplerData {
    // TODO: copy fastgltf::Sampler
  };
  /** \brief Texture composed of image and sampler. */
  struct TextureData {
    ImageData image;
    SamplerData sampler;
  };
  /** \brief Minimal physically-based material description. */
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
  
  /**
   * \brief Compiled shader artifact.
   *
   * Holds backend-specific code (SPIR-V) produced by \ref compile.
   */
  struct Shader {
    Slang::ComPtr<slang::IBlob> spirv;
  
    Shader() = default;
    /** \brief Construct and compile the shader at the given path. */
    Shader(const std::filesystem::path& path);
  };
  
  /** \brief Placeholder camera description. */
  struct Camera {};
  
  /** \brief Directional light parameters. */
  struct DirectionalLight {
    Color color;
    float intensity;
  };
  /** \brief Spot light parameters. */
  struct SpotLight {
    Color color;
    float intensity;
  };
  /** \brief Point light parameters. */
  struct PointLight {
    Color color;
    float intensity;
    float radius;
  };
  
  /**
   * \brief Aggregate renderable asset produced by the importer.
   *
   * Contains geometry and material data extracted from source files (e.g. glTF).
   */
  struct Asset {
    std::vector<Mesh> meshes;
    std::vector<MaterialData> materials;
  
    Asset() = default;
    /** \brief Construct and import an asset from the given file path. */
    Asset(const std::filesystem::path& path);
  };
} // namespace importer
} // namespace mr
