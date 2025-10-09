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
  template <typename T>
  T* steal_memory(std::vector<T>& victim)
  {
    union Theft
    {
      std::vector<T> target;
      ~Theft() {}
    } place_for_crime = {std::move(victim)};
    return place_for_crime.target.data();
  }

  /**
   * Parse a glTF file into a fastgltf::Asset.
   *
   * On IO or parse error, logs an error with the fastgltf code and returns
   * std::nullopt. Uses LoadExternalBuffers/Images to resolve external data.
   */
  static std::optional<fastgltf::Asset> get_asset_from_path(const std::filesystem::path &dir, const std::filesystem::path &path)
  {
    using namespace fastgltf;

    auto [err, data] = GltfDataBuffer::FromPath(path);
    if (err != Error::None) {
      MR_ERROR("Failed to parse GLTF file\n"
               "\t\t{}: {}", getErrorName(err), getErrorMessage(err));
      return std::nullopt; // Failed to load GLTF data
    }

    auto extensions = fastgltf::Extensions::KHR_lights_punctual
                    | fastgltf::Extensions::MSFT_texture_dds
                    | fastgltf::Extensions::MSFT_packing_occlusionRoughnessMetallic
                    | fastgltf::Extensions::None
                    ;
    Parser parser(extensions);
    auto options = fastgltf::Options::LoadExternalBuffers
                 | fastgltf::Options::DontRequireValidAssetMember
                 ;
    
    MR_DEBUG("Loading from directory {}", dir.string());

    auto [error, asset] = parser.loadGltf(data, dir, options);
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
  static std::optional<Mesh> get_mesh_from_primitive(const fastgltf::Asset &asset, const fastgltf::Primitive &primitive)
  {
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
        auto& idxAccessor = asset.accessors[primitive.indicesAccessor.value()];
        mesh.indices.resize(idxAccessor.count);
        fastgltf::copyFromAccessor<std::uint32_t>(asset, idxAccessor, mesh.indices.data());

        mesh.lods.resize(1);
        mesh.lods[0].indices = IndexSpan(mesh.indices);
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
    return format_byte_size(format);
  }

  /**
   * Decode a glTF image into linear RGBA float pixels using stb_image.
   *
   * Supports URI, embedded vector, and buffer view sources. Returns an
   * ImageData with owned memory; logs warnings for unexpected sources.
   */
  static std::optional<ImageData> get_image_from_gltf(const std::filesystem::path& directory, const fastgltf::Asset &asset, const fastgltf::Image &image)
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

          std::filesystem::path absolute_path = directory / filePath.uri.fspath();
          const std::string path = std::move(absolute_path).string();

          std::byte *image_pixels = nullptr;
          if (filePath.mimeType == fastgltf::MimeType::DDS || filePath.uri.fspath().extension() == ".dds") {
            dds::Image image;
            dds::ReadResult res = dds::readFile(path, &image);
            ASSERT(res == dds::ReadResult::Success, "Unable to parse DDS image", res);
            ASSERT(image.data.size() > 0, "Unable to load DDS image", res);

            new_image.width = image.width;
            new_image.height = image.height;
            new_image.depth = image.arraySize;
            new_image.format = (vk::Format)dds::getVulkanFormat(image.format, image.supportsAlpha);
            nrChannels = image.data.size() / image.width / image.height; // assume each channel is a single byte
            new_image.mip_level = image.mipmaps.size();

            image_pixels = (std::byte*)steal_memory(image.data);
            new_image.pixels.reset(image_pixels);
            ASSERT(new_image.pixels.get() != nullptr, "Couldn't allocate pixels array");
          }
          else {
            image_pixels = (std::byte*)stbi_load(path.c_str(), &new_image.width, &new_image.height, &nrChannels, 0);
            if (nrChannels == 4) {
              new_image.format = vk::Format::eR8G8B8A8Uint;
            }
            else if (nrChannels == 3) {
              new_image.format = vk::Format::eR8G8B8Uint;
            }
            else if (nrChannels == 2) {
              new_image.format = vk::Format::eR8G8Uint;
            }
            else if (nrChannels == 1) {
              new_image.format = vk::Format::eR8Uint;
            }
          }
          ASSERT(new_image.width > 0, "Sanity check failed", image.name, path.c_str());
          ASSERT(new_image.height > 0, "Sanity check failed", image.name, path.c_str());
          ASSERT(nrChannels > 0, "Sanity check failed", image.name, path.c_str());

          new_image.pixels.reset(image_pixels);
          new_image.depth = 1;
        },
        [&](const fastgltf::sources::Array& array) {
          std::byte *image_pixels = (std::byte*)stbi_load_from_memory((uint8_t*)array.bytes.data(),
              static_cast<int>(array.bytes.size()), &new_image.width, &new_image.height, &nrChannels, 0);
          ASSERT(new_image.width > 0, "Sanity check failed", image.name);
          ASSERT(new_image.height > 0, "Sanity check failed", image.name);
          ASSERT(nrChannels > 0, "Sanity check failed", image.name);

          if (nrChannels == 4) {
            new_image.format = vk::Format::eR8G8B8A8Uint;
          }
          else if (nrChannels == 3) {
            new_image.format = vk::Format::eR8G8B8Uint;
          }
          else if (nrChannels == 2) {
            new_image.format = vk::Format::eR8G8Uint;
          }
          else if (nrChannels == 1) {
            new_image.format = vk::Format::eR8Uint;
          }

          new_image.pixels.reset(image_pixels);
          new_image.depth = 1;
        },
        [&](const fastgltf::sources::Vector& vector) {
          std::byte *image_pixels = (std::byte*)stbi_load_from_memory((uint8_t*)vector.bytes.data(),
              static_cast<int>(vector.bytes.size()), &new_image.width, &new_image.height, &nrChannels, 0);
          ASSERT(new_image.width > 0, "Sanity check failed", image.name);
          ASSERT(new_image.height > 0, "Sanity check failed", image.name);
          ASSERT(nrChannels > 0, "Sanity check failed", image.name);

          if (nrChannels == 4) {
            new_image.format = vk::Format::eR8G8B8A8Uint;
          }
          else if (nrChannels == 3) {
            new_image.format = vk::Format::eR8G8B8Uint;
          }
          else if (nrChannels == 2) {
            new_image.format = vk::Format::eR8G8Uint;
          }
          else if (nrChannels == 1) {
            new_image.format = vk::Format::eR8Uint;
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
                                                        &new_image.width, &new_image.height, &nrChannels, 0);
              ASSERT(new_image.width > 0, "Sanity check failed", image.name);
              ASSERT(new_image.height > 0, "Sanity check failed", image.name);
              ASSERT(nrChannels > 0, "Sanity check failed", image.name);

              if (nrChannels == 4) {
                new_image.format = vk::Format::eR8G8B8A8Uint;
              }
              else if (nrChannels == 3) {
                new_image.format = vk::Format::eR8G8B8Uint;
              }
              else if (nrChannels == 2) {
                new_image.format = vk::Format::eR8G8Uint;
              }
              else if (nrChannels == 1) {
                new_image.format = vk::Format::eR8Uint;
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
      const std::filesystem::path& directory,
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
    ImageData img_data = *ASSERT_VAL(get_image_from_gltf(directory, asset, img));

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
  static std::vector<MaterialData> get_materials_from_asset(
      const std::filesystem::path &directory,
      fastgltf::Asset *asset) {
    ASSERT(asset);

    std::vector<MaterialData> materials;
    materials.resize(asset->materials.size());

    tbb::parallel_for(0uz, asset->materials.size(),
      [&asset, &materials, &directory] (size_t i) {
        fastgltf::Material &src = asset->materials[i];
        MaterialData       &dst = materials[i];

        dst.constants.base_color_factor = color_from_nvec4(src.pbrData.baseColorFactor);
        dst.constants.roughness_factor = src.pbrData.roughnessFactor;
        dst.constants.metallic_factor = src.pbrData.metallicFactor;
        dst.constants.emissive_color = color_from_nvec3(src.emissiveFactor);
        dst.constants.normal_map_intensity = 1;
        dst.constants.emissive_strength = src.emissiveStrength;

        if (src.pbrData.baseColorTexture.has_value()) {
          auto exp = get_texture_from_gltf(directory, *asset, TextureType::BaseColor, src.pbrData.baseColorTexture.value());
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading Base Color texture - ", exp.error());
          }
        }

        if (src.normalTexture.has_value()) {
          auto exp = get_texture_from_gltf(directory, *asset, TextureType::NormalMap, src.normalTexture.value());
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
            directory,
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
            directory,
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
            auto exp = get_texture_from_gltf(directory, *asset, TextureType::OcclusionMap, src.occlusionTexture.value());
            if (exp.has_value()) {
              dst.textures.emplace_back(std::move(exp.value()));
            }
            else {
              MR_ERROR("Loading Occlusion texture - ", exp.error());
            }
          }
        }

        if (src.emissiveTexture.has_value()) {
          auto exp = get_texture_from_gltf(directory, *asset, TextureType::EmissiveColor, src.emissiveTexture.value());
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
    std::filesystem::path dir = path.parent_path();
    std::optional<fastgltf::Asset> asset = get_asset_from_path(dir, path);
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
      graph, tbb::flow::unlimited, [&res, &dir](fastgltf::Asset* asset) {
        res.materials = get_materials_from_asset(dir, asset);
      }
    };
    materials_load.try_put(&asset.value());

    graph.wait_for_all();

    return res;
  }
  }
}
