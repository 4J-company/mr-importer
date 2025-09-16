#pragma once

#include "def.hpp"

namespace mr {
inline namespace importer {
  /**
   * \file assets.hpp
   * \brief Core data structures owned and returned by the importer.
   */

  using PackedVec3f = std::array<float, 3>;

  /** \brief 3D position in object space. */
  using Position = PackedVec3f;
  /** \brief Index into vertex arrays. */
  using Index = std::uint32_t;
  /** \brief Local-to-world transform matrix. */
  using Transform = mr::Matr4f;
  /** \brief RGBA color in linear space. */
  using Color = mr::Color;
  /** \brief Per-vertex attributes used by the renderer. */
  struct VertexAttributes {
    Color color;
    PackedVec3f normal;
    PackedVec3f tangent;
    PackedVec3f bitangent;
    mr::Vec2f texcoord;
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
    std::size_t material;
  };

  // material-related data
  /** \brief Raw image data stored as linear RGBA float pixels. */
  struct ImageData {
    // unique ptr because memory is allocated by stb and passed to us
    std::unique_ptr<Color[]> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;

    ImageData() = default;
    ImageData(ImageData&&) noexcept = default;
    ImageData& operator=(ImageData&&) noexcept = default;
  };

  /** \brief Texture sampler settings placeholder. */
  struct SamplerData {
    // TODO: copy fastgltf::Sampler
  };

  /** \brief Adds PBR meaning to the texture */
  enum struct TextureType : uint32_t {
    BaseColor                  = 0,
    RoughnessMetallic          = 1,
    OcclusionRoughnessMetallic = 2,
    SpecularGlossiness         = 3,
    EmissiveColor              = 4,
    OcclusionMap               = 5,
    NormalMap                  = 6,

    Max
  };

  /** \brief Texture composed of image and sampler. */
  struct TextureData {
    ImageData image;
    TextureType type;
    SamplerData sampler;

    TextureData() = default;
    TextureData(TextureData&&) noexcept = default;
    TextureData& operator=(TextureData&&) noexcept = default;
  };

  /** \brief Minimal physically-based material description. */
  struct MaterialData {
    struct alignas(16) ConstantBlock {
      Color base_color_factor;
      Color emissive_color;
      float emissive_strength;
      float normal_map_intensity;

      float roughness_factor;
      float metallic_factor;
    };

    static inline constexpr size_t constants_bytesize = sizeof(ConstantBlock);

    ConstantBlock constants;
    std::vector<TextureData> textures;

    MaterialData() = default;
    MaterialData(MaterialData&&) noexcept = default;
    MaterialData& operator=(MaterialData&&) noexcept = default;

    constexpr std::span<const std::byte, constants_bytesize> constants_data() const noexcept {
      return std::span<const std::byte, constants_bytesize> {
        reinterpret_cast<const std::byte*>(&constants),
        constants_bytesize
      };
    }
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
  struct Model {
    std::vector<Mesh> meshes;
    std::vector<MaterialData> materials;

    Model() = default;
    /** \brief Construct and import an asset from the given file path. */
    Model(const std::filesystem::path& path);
  };
} // namespace importer
} // namespace mr
