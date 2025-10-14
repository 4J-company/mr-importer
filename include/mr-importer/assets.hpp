#pragma once

#include "def.hpp"

#include "helpers.hpp"

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
  using AABB = mr::AABBf;

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

  /** \brief Contiguous view on array of triangle indices. */
  struct IndexSpan : std::span<Index> {
    using std::span<Index>::span;
    using std::span<Index>::operator=;
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
      IndexSpan indices;
      IndexSpan shadow_indices;
    };

    PositionArray positions;
    IndexArray indices;
    VertexAttributesArray attributes;
    std::vector<LOD> lods;
    std::vector<Transform> transforms;
    std::string name;
    std::size_t material;
    AABB aabb;

    static inline constexpr std::array vertex_input_attribute_descriptions {
      vk::VertexInputAttributeDescription {
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 0
      },
      vk::VertexInputAttributeDescription {
        .location = 1,
        .binding = 1,
        .format = vk::Format::eR32G32B32A32Sfloat,
        .offset = offsetof(VertexAttributes, color)
      },
      vk::VertexInputAttributeDescription {
        .location = 2,
        .binding = 1,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(VertexAttributes, normal)
      },
      vk::VertexInputAttributeDescription {
        .location = 3,
        .binding = 1,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(VertexAttributes, tangent)
      },
      vk::VertexInputAttributeDescription {
        .location = 4,
        .binding = 1,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(VertexAttributes, bitangent)
      },
      vk::VertexInputAttributeDescription {
        .location = 5,
        .binding = 1,
        .format = vk::Format::eR32G32Sfloat,
        .offset = offsetof(VertexAttributes, texcoord)
      },
    };
  };


  // material-related data
  /** \brief Raw image data stored as linear RGBA float pixels. */
  struct ImageData {
    // unique ptr because memory is allocated by the backend and passed to us
    std::unique_ptr<std::byte[]> pixels;
    InplaceVector<std::span<const std::byte>, 16> mips;
    int32_t width = 0;
    int32_t height = 0;
    int32_t depth = 1;
    int32_t bytes_per_pixel = -1;
    vk::Format format {};

    ImageData() = default;
    ~ImageData() noexcept = default;
    ImageData(ImageData&&) noexcept = default;
    ImageData& operator=(ImageData&&) noexcept = default;
    ImageData(const ImageData&) noexcept = delete;
    ImageData& operator=(const ImageData&) noexcept = delete;

    uint32_t pixel_byte_size() const noexcept;
    constexpr uint32_t num_of_pixels() const noexcept { return width * height; }
    constexpr uint32_t byte_size() const noexcept { return num_of_pixels() * pixel_byte_size(); }
    constexpr mr::Extent extent() const noexcept { return {uint32_t(width), uint32_t(height)}; }
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
    TextureData(ImageData &&image, TextureType type, SamplerData sampler)
      : image(std::move(image)), type(type), sampler(sampler) {}
    TextureData(TextureData&&) noexcept = default;
    TextureData& operator=(TextureData&&) noexcept = default;
    TextureData(const TextureData&) noexcept = delete;
    TextureData& operator=(const TextureData&) noexcept = delete;
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
    MaterialData(const MaterialData&) noexcept = delete;
    MaterialData& operator=(const MaterialData&) noexcept = delete;

    constexpr std::span<const std::byte, constants_bytesize> constants_data() const noexcept {
      return std::span<const std::byte, constants_bytesize> {
        reinterpret_cast<const std::byte*>(&constants),
        constants_bytesize
      };
    }
  };

  // TODO: animation-related data

  template <typename T>
  struct SizedUniqueArray : public std::unique_ptr<T[]> {
    using std::unique_ptr<T[]>::unique_ptr;
    using std::unique_ptr<T[]>::operator=;

  private:
    size_t _size = 0;

  public:
    size_t size() const noexcept { return _size; }
    void size(size_t size) noexcept { _size = size; }
  };

  /**
   * \brief Compiled shader artifact.
   *
   * Holds backend-specific code (SPIR-V) produced by \ref compile.
   */
  struct Shader {
    SizedUniqueArray<const std::byte> spirv;

    Shader() = default;
    /** \brief Construct and compile the shader at the given path. */
    Shader(const std::filesystem::path& path);
  };

  struct LightBase {
    // RGB == Color; A == Intensity
    Color packed_color_and_intensity;

    LightBase(float r, float g, float b, float intensity)
      : packed_color_and_intensity(r, g, b, intensity)
    {}

    float intensity() const noexcept { return packed_color_and_intensity.a(); }
    Color color() const noexcept {
      return {
        packed_color_and_intensity.r(),
        packed_color_and_intensity.g(),
        packed_color_and_intensity.b(),
      };
    }
  };

  /** \brief Directional light parameters. */
  struct DirectionalLight : LightBase {
    using LightBase::LightBase;
    using LightBase::operator=;
  };
  /** \brief Spot light parameters. */
  struct SpotLight : LightBase {
    using LightBase::LightBase;
    using LightBase::operator=;

    float inner_cone_angle;
    float outer_cone_angle;

    SpotLight(float r, float g, float b, float intensity, float inner_angle, float outer_angle)
      : LightBase(r, g, b, intensity)
      , inner_cone_angle(inner_angle)
      , outer_cone_angle(outer_angle)
    {}
  };
  /** \brief Point light parameters. */
  struct PointLight : LightBase {
    using LightBase::LightBase;
    using LightBase::operator=;
  };

  /**
   * \brief Aggregate renderable asset produced by the importer.
   *
   * Contains geometry and material data extracted from source files (e.g. glTF).
   */
  struct Model {
    std::vector<Mesh> meshes;
    std::vector<MaterialData> materials;
    struct Lights {
      std::vector<DirectionalLight> directionals;
      std::vector<PointLight> points;
      std::vector<SpotLight> spots;
    } lights;

    Model() = default;
    /** \brief Construct and import an asset from the given file path. */
    Model(const std::filesystem::path& path);
  };
} // namespace importer
} // namespace mr
