/**
 * \file loader.cpp
 * \brief glTF loading and conversion into runtime asset structures.
 */

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <dds.hpp>

#include "mr-importer/importer.hpp"

#include "pch.hpp"

namespace mr {
inline namespace importer {
  /**
   * Parse a glTF file into a fastgltf::Asset.
   *
   * On IO or parse error, logs an error with the fastgltf code and returns
   * std::nullopt. Uses LoadExternalBuffers/Images to resolve external data.
   */
  static std::optional<fastgltf::Asset> get_asset_from_path(const std::filesystem::path &path) {
    using namespace fastgltf;

    auto [err, data] = GltfDataBuffer::FromPath(path);
    if (err != Error::None) {
      MR_ERROR("Failed to parse GLTF file\n"
               "\t\t{}: {}", getErrorName(err), getErrorMessage(err));
      return std::nullopt; // Failed to load GLTF data
    }

    auto extensions = fastgltf::Extensions::KHR_lights_punctual
                    | fastgltf::Extensions::None
                    ;
    Parser parser(extensions);
    auto options = fastgltf::Options::LoadExternalBuffers
                 | fastgltf::Options::LoadExternalImages
                 | fastgltf::Options::DontRequireValidAssetMember
                 ;
    
    auto [error, asset] = parser.loadGltf(data, path.parent_path(), options);
    if (error != Error::None) {
      MR_ERROR("Failed to parse GLTF file\n"
               "\t\t{}: {}", getErrorName(error), getErrorMessage(error));
      return std::nullopt; // Failed to load GLTF data
    }

    return std::move(asset);
  }

  /**
   * Locate an accessor by attribute name on a primitive and validate its type.
   *
   * Expects Vec3 Float for POSITION/NORMAL. Logs warnings when missing or of
   * unexpected type, and returns std::nullopt in that case.
   */
  static std::optional<std::reference_wrapper<const fastgltf::Accessor>> get_accessor_by_name(
    const fastgltf::Asset &asset,
    const fastgltf::Primitive &primitive,
    std::string_view name)
  {
    using namespace fastgltf;

    auto attr = primitive.findAttribute(name);
    if (attr == primitive.attributes.cend()) {
      MR_WARNING("primitive didn't contain {} attribute", name);
      return std::nullopt;
    }
    size_t acessor_id = attr->accessorIndex;
    if (acessor_id >= asset.accessors.size()) {
      MR_ERROR("primitive didn't contain {} accessor", name);
      return std::nullopt;
    }
    const Accessor& accessor = asset.accessors[acessor_id];
    if (!accessor.bufferViewIndex.has_value()) {
      MR_ERROR("primitive didn't contain buffer view");
      return std::nullopt;
    }

    return std::ref(accessor);
  }

  /**
   * Convert a glTF primitive into an internal Mesh.
   *
   * Populates positions, normals (if present), primary LOD indices, and
   * leaves attributes partially defaulted if normals are missing.
   * Asserts that indices accessor exists.
   */
  static std::optional<Mesh> get_mesh_from_primitive(const fastgltf::Asset &asset, const fastgltf::Primitive &primitive) {
    using namespace fastgltf;

    Mesh mesh;


    tbb::flow::graph graph;

    // Process POSITION attribute
    tbb::flow::function_node<const char *> position_load {
      graph, tbb::flow::unlimited, [&](const char *str) {
        std::optional<std::reference_wrapper<const Accessor>> positions = get_accessor_by_name(asset, primitive, str);
        if (positions.has_value()) {
          mesh.positions.reserve(positions.value().get().count);
          fastgltf::iterateAccessor<glm::vec3>(asset, positions.value(), [&](glm::vec3 v) {
            mesh.positions.push_back({v.x, v.y, v.z});
          });
        }
      }
    };
    position_load.try_put("POSITION");

    // Process NORMAL attribute
    tbb::flow::function_node<const char *> normal_load {
      graph, tbb::flow::unlimited, [&](const char *str) {
        std::optional<std::reference_wrapper<const Accessor>> normals = get_accessor_by_name(asset, primitive, str);
        if (normals.has_value()) {
          mesh.attributes.resize(normals.value().get().count);
          fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normals.value(), [&](glm::vec3 v, int index) {
            mesh.attributes[index].normal = {v.x, v.y, v.z};
          });
        }
      }
    };
    normal_load.try_put("NORMAL");

    // Process TEXCOORD_0 attribute
    tbb::flow::function_node<const char *> texcoord0_load {
      graph, tbb::flow::unlimited, [&](const char *str) {
        std::optional<std::reference_wrapper<const Accessor>> texcoords = get_accessor_by_name(asset, primitive, str);
        if (texcoords.has_value()) {
          mesh.attributes.resize(texcoords.value().get().count);
          fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, texcoords.value(), [&](glm::vec2 v, int index) {
            mesh.attributes[index].texcoord = {v.x, v.y};
          });
        }
      }
    };
    texcoord0_load.try_put("TEXCOORD_0");

    ASSERT(primitive.indicesAccessor.has_value());

    // Process indices
    tbb::flow::function_node<const char *> index_load {
      graph, tbb::flow::unlimited, [&](const char *) {
        mesh.lods.resize(1);
        auto& idxAccessor = asset.accessors[primitive.indicesAccessor.value()];
        mesh.lods[0].indices.resize(idxAccessor.count);
        fastgltf::copyFromAccessor<std::uint32_t>(asset, idxAccessor, mesh.lods[0].indices.data());
      }
    };
    index_load.try_put(nullptr);

    if (primitive.materialIndex) {
      mesh.material = primitive.materialIndex.value();
    }
    else {
      MR_ERROR("Mesh has no material specified");
    }

    graph.wait_for_all();

    return mesh;
  }

  /**
   * Extract meshes from the fastgltf asset and attach per-mesh transforms.
   *
   * Iterates scene nodes to gather transforms, then converts all primitives
   * into Mesh objects, preserving names.
   */
  static std::vector<Mesh> get_meshes_from_asset(fastgltf::Asset *asset) {
    ASSERT(asset);

    using namespace fastgltf;

    tbb::concurrent_vector<Mesh> res;

    std::vector<std::vector<Transform>> transforms;
    transforms.resize(asset->meshes.size());
    fastgltf::iterateSceneNodes(*asset, 0, fastgltf::math::fmat4x4(),
      [&](fastgltf::Node& node, fastgltf::math::fmat4x4 matrix) {
        if (node.meshIndex.has_value()) {
          glm::mat4 t = glm::make_mat4(matrix.data());
          transforms[*node.meshIndex].push_back({
            t[0][0], t[1][0], t[2][0], t[3][0],
            t[0][1], t[1][1], t[2][1], t[3][1],
            t[0][2], t[1][2], t[2][2], t[3][2],
            t[0][3], t[1][3], t[2][3], t[3][3],
          });
        }
      }
    );

    tbb::parallel_for<int>(0, asset->meshes.size(), [&] (int i) {
      const fastgltf::Mesh& gltfMesh = asset->meshes[i];
      tbb::parallel_for<int>(0, gltfMesh.primitives.size(), [&] (int j) {
        const auto& primitive = gltfMesh.primitives[j];
        std::optional<Mesh> mesh_opt = get_mesh_from_primitive(*asset, primitive);
        if (mesh_opt.has_value()) {
          mesh_opt->transforms = transforms[i];
          mesh_opt->name = gltfMesh.name;
          res.emplace_back(std::move(mesh_opt.value()));
        }
      });
    });

    return {res.begin(), res.end()};
  }


  uint32_t ImageData::pixel_byte_size() const noexcept
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
      default: ASSERT(false, "Unhandled vk::Format", format);                                           
    }

    return res;
  }

  /**
   * Decode a glTF image into linear RGBA float pixels using stb_image.
   *
   * Supports URI, embedded vector, and buffer view sources. Returns an
   * ImageData with owned memory; logs warnings for unexpected sources.
   */
  static std::optional<ImageData> get_image_from_gltf(const fastgltf::Asset &asset, const fastgltf::Image &image)
  {
    ImageData new_image {};
    int nrChannels = -1;

    std::visit(
      fastgltf::visitor {
        [](auto& arg) { ASSERT(false, "Unsupported image source in a GLTF file", arg); },
        [&](const fastgltf::sources::URI& filePath) {
          ASSERT(filePath.fileByteOffset == 0,
              "Offsets with files are not supported becaues plain STB doesn't support them.",
              filePath.uri.c_str());
          ASSERT(filePath.uri.isLocalPath(),
              "Tried to load an image from absolute path"
              " - we don't support that (local files only)",
              filePath.uri.c_str());

          const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());

          std::byte *image_pixels = nullptr;
          if (filePath.mimeType == fastgltf::MimeType::DDS) {
            dds::Image image;
            dds::ReadResult res = dds::readFile(path, &image);
            ASSERT(res == dds::ReadResult::Success, "Unable to parse DDS image", res);
            ASSERT(image.data.size() > 0, "Unable to load DDS image", res);

            new_image.width = image.width;
            new_image.height = image.height;
            new_image.depth = image.arraySize;
            new_image.format = (vk::Format)dds::getVulkanFormat(image.format, image.supportsAlpha);
            new_image.mip_level = image.mipmaps.size();

            new_image.pixels = std::make_unique_for_overwrite<std::byte[]>(image.data.size());
            ASSERT(new_image.pixels.get() != nullptr, "Couldn't allocate pixels array");
            std::memcpy(image.data.data(), new_image.pixels.get(), image.data.size());
          }
          else {
            image_pixels = (std::byte*)stbi_load(path.c_str(), &new_image.width, &new_image.height, &nrChannels, 3);
          }
          ASSERT(new_image.width > 0, "Sanity check failed", image.name, path.c_str());
          ASSERT(new_image.height > 0, "Sanity check failed", image.name, path.c_str());

          if (nrChannels != 3) {
            MR_WARNING("Image {} ({}) is not 3-component per pixel - it's {}-component. "
                       "Currently it's realigned inside stb every time it gets imported. "
                       "Please do it offline if possible", image.name, filePath.uri.c_str(), nrChannels);
          }

          new_image.pixels.reset(image_pixels);
          new_image.depth = 1;
        },
        [&](const fastgltf::sources::Array& array) {
          std::byte *image_pixels = (std::byte*)stbi_load_from_memory((uint8_t*)array.bytes.data(),
              static_cast<int>(array.bytes.size()), &new_image.width, &new_image.height, &nrChannels, 3);
          ASSERT(new_image.width > 0, "Sanity check failed", image.name);
          ASSERT(new_image.height > 0, "Sanity check failed", image.name);

          if (nrChannels != 3) {
            MR_WARNING("Image {} is not 3-component per pixel - it's {}-component. "
                       "Currently it's realigned inside stb every time it gets imported."
                       "Please do it offline if possible", image.name, nrChannels);
          }

          new_image.pixels.reset(image_pixels);
          new_image.depth = 1;
        },
        [&](const fastgltf::sources::Vector& vector) {
          std::byte *image_pixels = (std::byte*)stbi_load_from_memory((uint8_t*)vector.bytes.data(),
              static_cast<int>(vector.bytes.size()), &new_image.width, &new_image.height, &nrChannels, 3);
          ASSERT(new_image.width > 0, "Sanity check failed", image.name);
          ASSERT(new_image.height > 0, "Sanity check failed", image.name);

          if (nrChannels != 3) {
            MR_WARNING("Image {} is not 3-component per pixel - it's {}-component. "
                       "Currently it's realigned inside stb every time it gets imported."
                       "Please do it offline if possible", image.name, nrChannels);
          }

          new_image.pixels.reset(image_pixels);
          new_image.depth = 1;
        },
        [&](const fastgltf::sources::BufferView& view) {
          auto& bufferView = asset.bufferViews[view.bufferViewIndex];
          auto& buffer = asset.buffers[bufferView.bufferIndex];

          std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                         // specify LoadExternalBuffers, meaning all buffers
                                         // are already loaded into a vector.
            [](auto& arg) { ASSERT(false, "Try to process image from buffer view but not from RAM (should be illegal because of LoadExternalBuffers)"); },
            [&](fastgltf::sources::Vector& vector) {
            std::byte *image_pixels = (std::byte*)stbi_load_from_memory((uint8_t*)vector.bytes.data() + bufferView.byteOffset,
                                                        static_cast<int>(bufferView.byteLength),
                                                        &new_image.width, &new_image.height, &nrChannels, 3);
              ASSERT(new_image.width > 0, "Sanity check failed", image.name);
              ASSERT(new_image.height > 0, "Sanity check failed", image.name);

              if (nrChannels != 3) {
                MR_WARNING("Image {} is not 3-component per pixel - it's {}-component. "
                           "Currently it's realigned inside stb every time it gets imported. "
                           "Please do it offline if possible.", image.name, nrChannels);
              }

              new_image.pixels.reset(image_pixels);
              new_image.depth = 1;
            }
          }, buffer.data);
        },
      },
      image.data);

    ASSERT(new_image.pixels.get() != nullptr, "Unexpected error reading image data. Needs investigation", image.name);

    return new_image;
  }

  /**
   * Create a TextureData from a glTF TextureInfo, decoding its image.
   *
   * Returns TextureData on success or an explanatory string_view on failure
   * (e.g., unsupported formats). Does not throw.
   */
  static std::expected<TextureData, std::string_view> get_texture_from_gltf(
      fastgltf::Asset &asset,
      TextureType type,
      const fastgltf::TextureInfo &texinfo)
  {
    fastgltf::Texture &tex = asset.textures[texinfo.textureIndex];

    size_t img_idx = ~0z;

    if (tex.imageIndex.has_value()) {
      img_idx = tex.imageIndex.value();
    }
    if (tex.ddsImageIndex.has_value()) {
      img_idx = tex.ddsImageIndex.value();
    }
    if (img_idx == ~0z) {
      return std::unexpected("Texture is in unsupported format (KTX, WEBP, etc)");
    }

    fastgltf::Image &img = asset.images[img_idx];
    ImageData img_data = *ASSERT_VAL(get_image_from_gltf(asset, img));

    TextureData res;
    res.image = std::move(img_data);
    res.type = type;
    return res;
  }

  /** Convert normalized vec4 to Color. */
  static Color color_from_nvec4(fastgltf::math::nvec4 v) {
    return { v.x(), v.y(), v.z(), v.w() };
  }
  /** Convert normalized vec3 to Color with alpha = 1. */
  static Color color_from_nvec3(fastgltf::math::nvec3 v) {
    return { v.x(), v.y(), v.z(), 1.f };
  }

  /**
   * Build MaterialData array from glTF materials.
   *
   * Transfers PBR factors and attempts to load referenced textures; logs
   * errors/warnings for failed texture loads and continues gracefully.
   */
  static std::vector<MaterialData> get_materials_from_asset(fastgltf::Asset *asset) {
    ASSERT(asset);

    std::vector<MaterialData> materials;
    materials.resize(asset->materials.size());

    tbb::parallel_for(0uz, asset->materials.size(),
      [&asset, &materials] (size_t i) {
        fastgltf::Material &src = asset->materials[i];
        MaterialData       &dst = materials[i];

        dst.constants.base_color_factor = color_from_nvec4(src.pbrData.baseColorFactor);
        dst.constants.roughness_factor = src.pbrData.roughnessFactor;
        dst.constants.metallic_factor = src.pbrData.metallicFactor;
        dst.constants.emissive_color = color_from_nvec3(src.emissiveFactor);
        dst.constants.normal_map_intensity = 1;
        dst.constants.emissive_strength = src.emissiveStrength;

        if (src.pbrData.baseColorTexture.has_value()) {
          auto exp = get_texture_from_gltf(*asset, TextureType::BaseColor, src.pbrData.baseColorTexture.value());
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading Base Color texture - ", exp.error());
          }
        }

        if (src.normalTexture.has_value()) {
          auto exp = get_texture_from_gltf(*asset, TextureType::NormalMap, src.normalTexture.value());
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_WARNING("Loading Normal Map texture - ", exp.error());
          }
        }

        if (src.packedOcclusionRoughnessMetallicTextures &&
            src.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.has_value()) {
          auto exp = get_texture_from_gltf(
            *asset,
            TextureType::OcclusionRoughnessMetallic,
            src.packedOcclusionRoughnessMetallicTextures->occlusionRoughnessMetallicTexture.value()
          );
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading packed Occlusion Roughness Metallic texture - ", exp.error());
          }
        }
        else if (src.pbrData.metallicRoughnessTexture.has_value()) {
          auto exp = get_texture_from_gltf(
            *asset,
            TextureType::RoughnessMetallic,
            src.pbrData.metallicRoughnessTexture.value()
          );

          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading Metallic Roughness texture - ", exp.error());
          }

          if (src.occlusionTexture.has_value()) {
            auto exp = get_texture_from_gltf(*asset, TextureType::OcclusionMap, src.occlusionTexture.value());
            if (exp.has_value()) {
              dst.textures.emplace_back(std::move(exp.value()));
            }
            else {
              MR_ERROR("Loading Occlusion texture - ", exp.error());
            }
          }
        }

        if (src.emissiveTexture.has_value()) {
          auto exp = get_texture_from_gltf(*asset, TextureType::EmissiveColor, src.emissiveTexture.value());
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading Emissive texture - ", exp.error());
          }
        }
      }
    );

    return materials;
  }

  /**
   * Load a source asset (currently glTF) and convert it into runtime \ref Model.
   * Returns std::nullopt on parse or IO errors; logs details via MR_ logging.
   */
  std::optional<Model> load(std::filesystem::path path) {
    std::optional<fastgltf::Asset> asset = get_asset_from_path(path);
    if (!asset) {
      return std::nullopt;
    }

    importer::Model res;

    tbb::flow::graph graph;

    tbb::flow::function_node<fastgltf::Asset*> meshes_load {
      graph, tbb::flow::unlimited, [&res](fastgltf::Asset* asset) {
        res.meshes = get_meshes_from_asset(asset);
      }
    };
    meshes_load.try_put(&asset.value());

    tbb::flow::function_node<fastgltf::Asset*> materials_load {
      graph, tbb::flow::unlimited, [&res](fastgltf::Asset* asset) {
        res.materials = get_materials_from_asset(asset);
      }
    };
    materials_load.try_put(&asset.value());

    graph.wait_for_all();

    return res;
  }
  }
}
