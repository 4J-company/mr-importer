/**
 * \file loader.cpp
 * \brief glTF loading and conversion into runtime asset structures.
 */

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
      MR_ERROR("Failed to parse GLTF file");
      MR_ERROR("Error code: {}", (int)err);
      return std::nullopt; // Failed to load GLTF data
    }

    Parser parser;
    auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages;
    auto [error, asset] = parser.loadGltf(data, path.parent_path(), options);
    if (error != Error::None) {
      MR_ERROR("Failed to parse GLTF file");
      MR_ERROR("Error code: {}", (int)error);
      return std::nullopt;
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
    if (accessor.type != AccessorType::Vec3 || accessor.componentType != ComponentType::Float) {
      MR_ERROR("primitive's itions were in wrong format (not Vec3f)");
      return std::nullopt;
    }
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

    // Process POSITION attribute
    std::optional<std::reference_wrapper<const Accessor>> positions = get_accessor_by_name(asset, primitive, "POSITION");
    if (positions.has_value()) {
      mesh.positions.reserve(positions.value().get().count);
      fastgltf::iterateAccessor<glm::vec3>(asset, positions.value(), [&](glm::vec3 v) {
        mesh.positions.push_back(v);
      });
    }

    // Process NORMAL attribute
    std::optional<std::reference_wrapper<const Accessor>> normals = get_accessor_by_name(asset, primitive, "NORMAL");
    if (normals.has_value()) {
      mesh.attributes.resize(normals.value().get().count);
      fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normals.value(), [&](glm::vec3 v, int index) {
        mesh.attributes[index].normal = v;
      });
    }

    // Process indices
    assert(primitive.indicesAccessor.has_value());
    mesh.lods.resize(1);
    auto& idxAccessor = asset.accessors[primitive.indicesAccessor.value()];
    mesh.lods[0].indices.resize(idxAccessor.count);
    fastgltf::copyFromAccessor<std::uint32_t>(asset, idxAccessor, mesh.lods[0].indices.data());

    if (primitive.materialIndex) {
      mesh.material = primitive.materialIndex.value();
    }
    else {
      MR_ERROR("Mesh has no material specified");
    }

    return mesh;
  }

  /**
   * Extract meshes from the fastgltf asset and attach per-mesh transforms.
   *
   * Iterates scene nodes to gather transforms, then converts all primitives
   * into Mesh objects, preserving names.
   */
  static std::vector<Mesh> get_meshes_from_asset(fastgltf::Asset& asset) {
    using namespace fastgltf;

    std::vector<Mesh> result;

    std::vector<std::vector<glm::mat4>> transforms;
    transforms.resize(asset.meshes.size());
    fastgltf::iterateSceneNodes(asset, 0, fastgltf::math::fmat4x4(),
      [&](fastgltf::Node& node, fastgltf::math::fmat4x4 matrix) {
        if (node.meshIndex.has_value()) {
          transforms[*node.meshIndex].push_back(glm::make_mat4(matrix.data()));
        }
      }
    );

    for (int i = 0; i < asset.meshes.size(); i++) {
      const fastgltf::Mesh& gltfMesh = asset.meshes[i];
      for (int j = 0; j < gltfMesh.primitives.size(); j++) {
        const auto& primitive = gltfMesh.primitives[j];
        std::optional<Mesh> mesh_opt = get_mesh_from_primitive(asset, primitive);
        if (mesh_opt.has_value()) {
          mesh_opt->transforms = transforms[i];
          mesh_opt->name = gltfMesh.name;
          result.emplace_back(std::move(mesh_opt.value()));
        }
      }
    }

    return result;
  }

  /**
   * Decode a glTF image into linear RGBA float pixels using stb_image.
   *
   * Supports URI, embedded vector, and buffer view sources. Returns an
   * ImageData with owned memory; logs warnings for unexpected sources.
   */
  static std::optional<ImageData> get_image_from_gltf(const fastgltf::Asset &asset, const fastgltf::Image &image) {
    ImageData new_image {};

    int width, height, nrChannels;

    std::visit(
      fastgltf::visitor {
        [](auto& arg) {},
        [&](fastgltf::sources::URI& filePath) {
          assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
          assert(filePath.uri.isLocalPath());   // We're only capable of loading local files.

          const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());

          new_image.pixels.reset((Color*)stbi_loadf(path.c_str(), &width, &height, &nrChannels, 4));
          new_image.width = width;
          new_image.height = height;
          new_image.depth = 1;
        },
        [&](fastgltf::sources::Vector& vector) {
          new_image.pixels.reset((Color*)stbi_loadf_from_memory((uint8_t*)vector.bytes.data(),
                                 static_cast<int>(vector.bytes.size()),
                                 &width, &height, &nrChannels, 4));
          new_image.width = width;
          new_image.height = height;
          new_image.depth = 1;
        },
        [&](fastgltf::sources::BufferView& view) {
          auto& bufferView = asset.bufferViews[view.bufferViewIndex];
          auto& buffer = asset.buffers[bufferView.bufferIndex];

          std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                         // specify LoadExternalBuffers, meaning all buffers
                                         // are already loaded into a vector.
            [](auto& arg) { MR_WARNING("Try to process image from buffer view but not from RAM (should be illegal because of LoadExternalBuffers)"); },
            [&](fastgltf::sources::Vector& vector) {
              new_image.pixels.reset((Color*)stbi_loadf_from_memory((uint8_t*)vector.bytes.data() + bufferView.byteOffset,
                                                        static_cast<int>(bufferView.byteLength),
                                                        &width, &height, &nrChannels, 4));
              new_image.width = width;
              new_image.height = height;
              new_image.depth = 1;
            }
          }, buffer.data);
        },
      },
      image.data);

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

    if (!tex.imageIndex.has_value()) {
      return std::unexpected("Texture is in unsupported format (DDS, WEBP, etc)");
    }

    size_t img_idx = tex.imageIndex.value();

    fastgltf::Image &img = asset.images[img_idx];
    ImageData img_data = *ASSERT_VAL(get_image_from_gltf(asset, img));

    return TextureData { std::move(img_data), type, SamplerData {} };
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
  static std::vector<MaterialData> get_materials_from_asset(fastgltf::Asset &asset) {
    std::vector<MaterialData> materials;
    materials.resize(asset.materials.size());

    auto io = std::ranges::iota_view {0uz, asset.materials.size()};
    std::for_each(std::execution::seq, io.begin(), io.end(), [&asset, &materials] (size_t i) {
      fastgltf::Material &src = asset.materials[i];
      MaterialData       &dst = materials[i];
      
      dst.base_color_factor = color_from_nvec4(src.pbrData.baseColorFactor);
      dst.roughness_factor = src.pbrData.roughnessFactor;
      dst.metallic_factor = src.pbrData.metallicFactor;
      dst.emissive_color = color_from_nvec3(src.emissiveFactor);
      dst.normal_map_intensity = 1;
      dst.emissive_strength = src.emissiveStrength;

      if (src.pbrData.baseColorTexture.has_value()) {
        auto exp = get_texture_from_gltf(asset, TextureType::BaseColor, src.pbrData.baseColorTexture.value());
        if (exp.has_value()) {
          dst.textures.emplace_back(std::move(exp.value()));
        }
        else {
          MR_ERROR("Loading Base Color texture - ", exp.error());
        }
      }

      if (src.normalTexture.has_value()) {
        auto exp = get_texture_from_gltf(asset, TextureType::NormalMap, src.normalTexture.value());
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
          asset,
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
          asset,
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
          auto exp = get_texture_from_gltf(asset, TextureType::OcclusionMap, src.occlusionTexture.value());
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading Occlusion texture - ", exp.error());
          }
        }
      }

      if (src.emissiveTexture.has_value()) {
        auto exp = get_texture_from_gltf(asset, TextureType::EmissiveColor, src.emissiveTexture.value());
        if (exp.has_value()) {
          dst.textures.emplace_back(std::move(exp.value()));
        }
        else {
          MR_ERROR("Loading Emissive texture - ", exp.error());
        }
      }
    });

    return materials;
  }

  // Sequence:
  //   - parse gltf using fastgltf
  //   - Parallel:
  //     - mesh processing (DynamicParallel):
  //       - Sequence:
  //         - extract from gltf.bufferview's into PositionArray, IndexArray, VertexAttributesArray
  //         - optimize mesh data using meshoptimizer
  //     - material processing (DynamicParallel)
  //       - Sequence:
  //         - Parallel:
  //           - extract sampler data into SamplerData
  //           - extract from texture URI into ImageData using stb
  //         - compose into TextureData
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

    res.meshes = get_meshes_from_asset(asset.value());
    res.materials = get_materials_from_asset(asset.value());

    return res;
  }
  }
}
