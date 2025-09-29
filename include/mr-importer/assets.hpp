#pragma once

#include "def.hpp"

namespace mr {
inline namespace importer {
  constexpr uint32_t format_byte_size(vk::Format format)
  {
    uint32_t res = 0;

    switch ((VkFormat)format) {
      case VK_FORMAT_UNDEFINED: res = 0; break;
      case VK_FORMAT_R4G4_UNORM_PACK8: res = 1; break;
      case VK_FORMAT_R4G4B4A4_UNORM_PACK16: res = 2; break;
      case VK_FORMAT_B4G4R4A4_UNORM_PACK16: res = 2; break;
      case VK_FORMAT_R5G6B5_UNORM_PACK16: res = 2; break;
      case VK_FORMAT_B5G6R5_UNORM_PACK16: res = 2; break;
      case VK_FORMAT_R5G5B5A1_UNORM_PACK16: res = 2; break;
      case VK_FORMAT_B5G5R5A1_UNORM_PACK16: res = 2; break;
      case VK_FORMAT_A1R5G5B5_UNORM_PACK16: res = 2; break;
      case VK_FORMAT_R8_UNORM: res = 1; break;
      case VK_FORMAT_R8_SNORM: res = 1; break;
      case VK_FORMAT_R8_USCALED: res = 1; break;
      case VK_FORMAT_R8_SSCALED: res = 1; break;
      case VK_FORMAT_R8_UINT: res = 1; break;
      case VK_FORMAT_R8_SINT: res = 1; break;
      case VK_FORMAT_R8_SRGB: res = 1; break;
      case VK_FORMAT_R8G8_UNORM: res = 2; break;
      case VK_FORMAT_R8G8_SNORM: res = 2; break;
      case VK_FORMAT_R8G8_USCALED: res = 2; break;
      case VK_FORMAT_R8G8_SSCALED: res = 2; break;
      case VK_FORMAT_R8G8_UINT: res = 2; break;
      case VK_FORMAT_R8G8_SINT: res = 2; break;
      case VK_FORMAT_R8G8_SRGB: res = 2; break;
      case VK_FORMAT_R8G8B8_UNORM: res = 3; break;
      case VK_FORMAT_R8G8B8_SNORM: res = 3; break;
      case VK_FORMAT_R8G8B8_USCALED: res = 3; break;
      case VK_FORMAT_R8G8B8_SSCALED: res = 3; break;
      case VK_FORMAT_R8G8B8_UINT: res = 3; break;
      case VK_FORMAT_R8G8B8_SINT: res = 3; break;
      case VK_FORMAT_R8G8B8_SRGB: res = 3; break;
      case VK_FORMAT_B8G8R8_UNORM: res = 3; break;
      case VK_FORMAT_B8G8R8_SNORM: res = 3; break;
      case VK_FORMAT_B8G8R8_USCALED: res = 3; break;
      case VK_FORMAT_B8G8R8_SSCALED: res = 3; break;
      case VK_FORMAT_B8G8R8_UINT: res = 3; break;
      case VK_FORMAT_B8G8R8_SINT: res = 3; break;
      case VK_FORMAT_B8G8R8_SRGB: res = 3; break;
      case VK_FORMAT_R8G8B8A8_UNORM: res = 4; break;
      case VK_FORMAT_R8G8B8A8_SNORM: res = 4; break;
      case VK_FORMAT_R8G8B8A8_USCALED: res = 4; break;
      case VK_FORMAT_R8G8B8A8_SSCALED: res = 4; break;
      case VK_FORMAT_R8G8B8A8_UINT: res = 4; break;
      case VK_FORMAT_R8G8B8A8_SINT: res = 4; break;
      case VK_FORMAT_R8G8B8A8_SRGB: res = 4; break;
      case VK_FORMAT_B8G8R8A8_UNORM: res = 4; break;
      case VK_FORMAT_B8G8R8A8_SNORM: res = 4; break;
      case VK_FORMAT_B8G8R8A8_USCALED: res = 4; break;
      case VK_FORMAT_B8G8R8A8_SSCALED: res = 4; break;
      case VK_FORMAT_B8G8R8A8_UINT: res = 4; break;
      case VK_FORMAT_B8G8R8A8_SINT: res = 4; break;
      case VK_FORMAT_B8G8R8A8_SRGB: res = 4; break;
      case VK_FORMAT_A8B8G8R8_UNORM_PACK32: res = 4; break;
      case VK_FORMAT_A8B8G8R8_SNORM_PACK32: res = 4; break;
      case VK_FORMAT_A8B8G8R8_USCALED_PACK32: res = 4; break;
      case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: res = 4; break;
      case VK_FORMAT_A8B8G8R8_UINT_PACK32: res = 4; break;
      case VK_FORMAT_A8B8G8R8_SINT_PACK32: res = 4; break;
      case VK_FORMAT_A8B8G8R8_SRGB_PACK32: res = 4; break;
      case VK_FORMAT_A2R10G10B10_UNORM_PACK32: res = 4; break;
      case VK_FORMAT_A2R10G10B10_SNORM_PACK32: res = 4; break;
      case VK_FORMAT_A2R10G10B10_USCALED_PACK32: res = 4; break;
      case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: res = 4; break;
      case VK_FORMAT_A2R10G10B10_UINT_PACK32: res = 4; break;
      case VK_FORMAT_A2R10G10B10_SINT_PACK32: res = 4; break;
      case VK_FORMAT_A2B10G10R10_UNORM_PACK32: res = 4; break;
      case VK_FORMAT_A2B10G10R10_SNORM_PACK32: res = 4; break;
      case VK_FORMAT_A2B10G10R10_USCALED_PACK32: res = 4; break;
      case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: res = 4; break;
      case VK_FORMAT_A2B10G10R10_UINT_PACK32: res = 4; break;
      case VK_FORMAT_A2B10G10R10_SINT_PACK32: res = 4; break;
      case VK_FORMAT_R16_UNORM: res = 2; break;
      case VK_FORMAT_R16_SNORM: res = 2; break;
      case VK_FORMAT_R16_USCALED: res = 2; break;
      case VK_FORMAT_R16_SSCALED: res = 2; break;
      case VK_FORMAT_R16_UINT: res = 2; break;
      case VK_FORMAT_R16_SINT: res = 2; break;
      case VK_FORMAT_R16_SFLOAT: res = 2; break;
      case VK_FORMAT_R16G16_UNORM: res = 4; break;
      case VK_FORMAT_R16G16_SNORM: res = 4; break;
      case VK_FORMAT_R16G16_USCALED: res = 4; break;
      case VK_FORMAT_R16G16_SSCALED: res = 4; break;
      case VK_FORMAT_R16G16_UINT: res = 4; break;
      case VK_FORMAT_R16G16_SINT: res = 4; break;
      case VK_FORMAT_R16G16_SFLOAT: res = 4; break;
      case VK_FORMAT_R16G16B16_UNORM: res = 6; break;
      case VK_FORMAT_R16G16B16_SNORM: res = 6; break;
      case VK_FORMAT_R16G16B16_USCALED: res = 6; break;
      case VK_FORMAT_R16G16B16_SSCALED: res = 6; break;
      case VK_FORMAT_R16G16B16_UINT: res = 6; break;
      case VK_FORMAT_R16G16B16_SINT: res = 6; break;
      case VK_FORMAT_R16G16B16_SFLOAT: res = 6; break;
      case VK_FORMAT_R16G16B16A16_UNORM: res = 8; break;
      case VK_FORMAT_R16G16B16A16_SNORM: res = 8; break;
      case VK_FORMAT_R16G16B16A16_USCALED: res = 8; break;
      case VK_FORMAT_R16G16B16A16_SSCALED: res = 8; break;
      case VK_FORMAT_R16G16B16A16_UINT: res = 8; break;
      case VK_FORMAT_R16G16B16A16_SINT: res = 8; break;
      case VK_FORMAT_R16G16B16A16_SFLOAT: res = 8; break;
      case VK_FORMAT_R32_UINT: res = 4; break;
      case VK_FORMAT_R32_SINT: res = 4; break;
      case VK_FORMAT_R32_SFLOAT: res = 4; break;
      case VK_FORMAT_R32G32_UINT: res = 8; break;
      case VK_FORMAT_R32G32_SINT: res = 8; break;
      case VK_FORMAT_R32G32_SFLOAT: res = 8; break;
      case VK_FORMAT_R32G32B32_UINT: res = 12; break;
      case VK_FORMAT_R32G32B32_SINT: res = 12; break;
      case VK_FORMAT_R32G32B32_SFLOAT: res = 12; break;
      case VK_FORMAT_R32G32B32A32_UINT: res = 16; break;
      case VK_FORMAT_R32G32B32A32_SINT: res = 16; break;
      case VK_FORMAT_R32G32B32A32_SFLOAT: res = 16; break;
      case VK_FORMAT_R64_UINT: res = 8; break;
      case VK_FORMAT_R64_SINT: res = 8; break;
      case VK_FORMAT_R64_SFLOAT: res = 8; break;
      case VK_FORMAT_R64G64_UINT: res = 16; break;
      case VK_FORMAT_R64G64_SINT: res = 16; break;
      case VK_FORMAT_R64G64_SFLOAT: res = 16; break;
      case VK_FORMAT_R64G64B64_UINT: res = 24; break;
      case VK_FORMAT_R64G64B64_SINT: res = 24; break;
      case VK_FORMAT_R64G64B64_SFLOAT: res = 24; break;
      case VK_FORMAT_R64G64B64A64_UINT: res = 32; break;
      case VK_FORMAT_R64G64B64A64_SINT: res = 32; break;
      case VK_FORMAT_R64G64B64A64_SFLOAT: res = 32; break;
      case VK_FORMAT_B10G11R11_UFLOAT_PACK32: res = 4; break;
      case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: res = 4; break;
      case VK_FORMAT_D16_UNORM: res = 2; break;
      case VK_FORMAT_X8_D24_UNORM_PACK32: res = 4; break;
      case VK_FORMAT_D32_SFLOAT: res = 4; break;
      case VK_FORMAT_S8_UINT: res = 1; break;
      case VK_FORMAT_D16_UNORM_S8_UINT: res = 3; break;
      case VK_FORMAT_D24_UNORM_S8_UINT: res = 4; break;
      case VK_FORMAT_D32_SFLOAT_S8_UINT: res = 8; break;
      case VK_FORMAT_BC1_RGB_UNORM_BLOCK: res = 8; break;
      case VK_FORMAT_BC1_RGB_SRGB_BLOCK: res = 8; break;
      case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: res = 8; break;
      case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: res = 8; break;
      case VK_FORMAT_BC2_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_BC2_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_BC3_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_BC3_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_BC4_UNORM_BLOCK: res = 8; break;
      case VK_FORMAT_BC4_SNORM_BLOCK: res = 8; break;
      case VK_FORMAT_BC5_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_BC5_SNORM_BLOCK: res = 16; break;
      case VK_FORMAT_BC6H_UFLOAT_BLOCK: res = 16; break;
      case VK_FORMAT_BC6H_SFLOAT_BLOCK: res = 16; break;
      case VK_FORMAT_BC7_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_BC7_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: res = 8; break;
      case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: res = 8; break;
      case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: res = 8; break;
      case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: res = 8; break;
      case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_EAC_R11_UNORM_BLOCK: res = 8; break;
      case VK_FORMAT_EAC_R11_SNORM_BLOCK: res = 8; break;
      case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: res = 16; break;
      case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: res = 16; break;
      case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: res = 8; break;
      case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: res = 8; break;
      case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: res = 8; break;
      case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: res = 8; break;
      case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: res = 8; break;
      case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: res = 8; break;
      case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: res = 8; break;
      case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: res = 8; break;
      case VK_FORMAT_G8B8G8R8_422_UNORM_KHR: res = 4; break;
      case VK_FORMAT_B8G8R8G8_422_UNORM_KHR: res = 4; break;
      case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR: res = 8; break;
      case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR: res = 8; break;
      case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR: res = 8; break;
      case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16_KHR: res = 8; break;
      case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR: res = 8; break;
      case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR: res = 8; break;
      case VK_FORMAT_G16B16G16R16_422_UNORM_KHR: res = 8; break;
      case VK_FORMAT_B16G16R16G16_422_UNORM_KHR: res = 8; break;
      default: ASSERT(false, "Unhandled vk::Format", (int)format);                                           
    }

    return res;
  }


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
        .offset = 0
      },
      vk::VertexInputAttributeDescription {
        .location = 2,
        .binding = 1,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 16
      },
      vk::VertexInputAttributeDescription {
        .location = 3,
        .binding = 1,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 28
      },
      vk::VertexInputAttributeDescription {
        .location = 4,
        .binding = 1,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = 40
      },
      vk::VertexInputAttributeDescription {
        .location = 5,
        .binding = 1,
        .format = vk::Format::eR32G32Sfloat,
        .offset = 56
      },
    };

    static_assert(
      // Validate individual attribute descriptions with offsets
      vertex_input_attribute_descriptions[0].location == 0 &&
      vertex_input_attribute_descriptions[0].binding == 0 &&
      vertex_input_attribute_descriptions[0].format == vk::Format::eR32G32B32Sfloat &&
      vertex_input_attribute_descriptions[0].offset == 0 &&
      
      vertex_input_attribute_descriptions[1].location == 1 &&
      vertex_input_attribute_descriptions[1].binding == 1 &&
      vertex_input_attribute_descriptions[1].format == vk::Format::eR32G32B32A32Sfloat &&
      vertex_input_attribute_descriptions[1].offset == offsetof(VertexAttributes, color) && // 0
      
      vertex_input_attribute_descriptions[2].location == 2 &&
      vertex_input_attribute_descriptions[2].binding == 1 &&
      vertex_input_attribute_descriptions[2].format == vk::Format::eR32G32B32Sfloat &&
      vertex_input_attribute_descriptions[2].offset == offsetof(VertexAttributes, normal) && // 16
      
      vertex_input_attribute_descriptions[3].location == 3 &&
      vertex_input_attribute_descriptions[3].binding == 1 &&
      vertex_input_attribute_descriptions[3].format == vk::Format::eR32G32B32Sfloat &&
      vertex_input_attribute_descriptions[3].offset == offsetof(VertexAttributes, tangent) && // 28
      
      vertex_input_attribute_descriptions[4].location == 4 &&
      vertex_input_attribute_descriptions[4].binding == 1 &&
      vertex_input_attribute_descriptions[4].format == vk::Format::eR32G32B32Sfloat &&
      vertex_input_attribute_descriptions[4].offset == offsetof(VertexAttributes, bitangent) && // 40
      
      vertex_input_attribute_descriptions[5].location == 5 &&
      vertex_input_attribute_descriptions[5].binding == 1 &&
      vertex_input_attribute_descriptions[5].format == vk::Format::eR32G32Sfloat &&
      vertex_input_attribute_descriptions[5].offset == offsetof(VertexAttributes, texcoord) && // 56, NOT 52!
      
      // Validate member sizes
      sizeof(Color) == 16 &&           // 4x float32 = 16 bytes
      sizeof(PackedVec3f) == 12 &&     // 3x float32 = 12 bytes  
      sizeof(mr::Vec2f) == 8 &&        // 2x float32 = 8 bytes
      
      // Validate the actual memory layout with padding
      offsetof(VertexAttributes, color) == 0 &&
      offsetof(VertexAttributes, normal) == 16 &&
      offsetof(VertexAttributes, tangent) == 28 &&
      offsetof(VertexAttributes, bitangent) == 40 &&
      offsetof(VertexAttributes, texcoord) == 56 && // This reveals 4 bytes of padding!
      
      // Validate total struct size accounts for padding
      sizeof(VertexAttributes) == 64 && // 56 + 8 = 64 due to struct alignment
      
      // Verify the padding calculation
      (offsetof(VertexAttributes, texcoord) - 
       (offsetof(VertexAttributes, bitangent) + sizeof(PackedVec3f))) == 4, // 4 bytes padding
      
      "Vertex input attribute descriptions do not match actual struct layout and alignment"
    );

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
    std::unique_ptr<std::byte[]> pixels;
    int32_t width = 0;
    int32_t height = 0;
    int32_t depth = 1;
    int32_t mip_level = 1;
    vk::Format format = vk::Format::eR8G8B8Uint;

    ImageData() = default;
    ImageData(ImageData&&) noexcept = default;
    ImageData& operator=(ImageData&&) noexcept = default;

    uint32_t pixel_byte_size() const noexcept;
    constexpr uint32_t num_of_pixels() const noexcept { return width * height / pixel_byte_size(); }
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
