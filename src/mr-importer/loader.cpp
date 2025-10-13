/**
 * \file loader.cpp
 * \brief glTF loading and conversion into runtime asset structures.
 */

#define WUFFS_IMPLEMENTATION
#include "wuffs-v0.4.c"

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
                    | fastgltf::Extensions::KHR_materials_pbrSpecularGlossiness
                    | fastgltf::Extensions::EXT_mesh_gpu_instancing
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
    return bytes_per_pixel == -1 ? format_byte_size(format) : bytes_per_pixel;
  }

  static void resize_image(
      ImageData &image,
      size_t component_number,
      size_t component_size,
      size_t desired_component_number)
  {
    if (desired_component_number == component_number) {
      return;
    }

    size_t pixel_size = image.pixel_byte_size();
    size_t pixel_count = image.num_of_pixels();

    size_t desired_pixel_size = desired_component_number * component_size;

    size_t desired_byte_size = image.byte_size() / component_number * desired_component_number;
    std::unique_ptr<std::byte[]> new_ptr = std::make_unique<std::byte[]>(desired_byte_size);

    for (int i = 0; i < pixel_count; i++) {
      size_t pixel_byte_offset = i * pixel_size;
      size_t desired_pixel_byte_offset = i * desired_pixel_size;
      for (int j = 0; j < std::min(component_number, desired_component_number); j++) {
        for (int k = 0; k < component_size; k++) {
          new_ptr[desired_pixel_byte_offset + j * component_size + k] = image.pixels[pixel_byte_offset + j * component_size + k];
        }
      }
    }

    image.pixels = std::move(new_ptr);

    int offset = 0;
    for (int i = 0; i < image.mips.size(); i++) {
      size_t desired_mip_size = image.mips[i].size() / component_number * desired_component_number;
      image.mips[i] = {image.pixels.get() + offset, desired_mip_size};
      offset += desired_mip_size;
    }
  }

  /**
   * Decode a glTF image.
   *
   * Supports URI, embedded vector, and buffer view sources. Returns an
   * ImageData with owned memory; logs warnings for unexpected sources.
   */
  static std::optional<ImageData> get_image_from_gltf(const std::filesystem::path& directory, Options options, const fastgltf::Asset &asset, const fastgltf::Image &image)
  {
    ImageData new_image {};

    std::visit(
      fastgltf::visitor {
        [](auto& arg) { ASSERT(false, "Unsupported image source in a GLTF file", arg); },
        [&](const fastgltf::sources::URI& filePath) {
          ASSERT(filePath.fileByteOffset == 0,
              "Offsets with files are not supported becaues plain wuffs' C++ bindings don't support them.",
              filePath.uri.c_str());
          ASSERT(filePath.uri.isLocalPath(),
              "Tried to load an image from absolute path"
              " - we don't support that (local files only)",
              filePath.uri.c_str());

          std::filesystem::path absolute_path = directory / filePath.uri.fspath();
          const std::string path = std::move(absolute_path).string();

          if (filePath.mimeType == fastgltf::MimeType::DDS || filePath.uri.fspath().extension() == ".dds") {
            dds::Image dds_image;
            dds::ReadResult res = dds::readFile(path, &dds_image);
            ASSERT(res == dds::ReadResult::Success, "Unable to parse DDS image", res);
            ASSERT(dds_image.data.get() != nullptr, "Unable to load DDS image", res);
            ASSERT(dds::getBitsPerPixel(dds_image.format) % 8 == 0,
                "DDS image format bits_per_pixel % 8 != 0. "
                "To handle such cases you'd need to change public API from byte_size to bit_size");

            new_image.width = dds_image.width;
            new_image.height = dds_image.height;
            new_image.depth = dds_image.arraySize;
            new_image.format = (vk::Format)dds::getVulkanFormat(dds_image.format, dds_image.supportsAlpha);
            new_image.bytes_per_pixel = dds::getBitsPerPixel(dds_image.format) / 8;

            new_image.pixels.reset((std::byte*)dds_image.data.release());
            for (auto &mip : dds_image.mipmaps) {
              new_image.mips.emplace_back(std::as_bytes(mip));
            }
            ASSERT(new_image.width > 0, "Sanity check failed", image.name, path.c_str());
            ASSERT(new_image.height > 0, "Sanity check failed", image.name, path.c_str());
            ASSERT(new_image.bytes_per_pixel > 0, "Sanity check failed", image.name, path.c_str());
          }
          else {
            std::unique_ptr<FILE, decltype(&fclose)> file (fopen(path.c_str(), "rb"), fclose);

            ASSERT(file.get() != nullptr, "Unable to open image file for reading", path);

            wuffs_aux::DecodeImageCallbacks callbacks;
            wuffs_aux::sync_io::FileInput input(file.get());
            wuffs_aux::DecodeImageResult img = wuffs_aux::DecodeImage(callbacks, input);

            ASSERT(img.error_message.empty(), img.error_message, path);
            ASSERT(img.pixbuf.pixcfg.is_valid(), "Something is wrong with the image (idk :) )", path);

            wuffs_base__table_u8 tab = img.pixbuf.plane(0);
            wuffs_base__pixel_format format = img.pixbuf.pixel_format();

            ASSERT(format.bits_per_pixel() % 8 == 0,
                "Image format bits_per_pixel % 8 != 0. "
                "To handle such cases you'd need to change public API from byte_size to bit_size");

            new_image.bytes_per_pixel = format.bits_per_pixel() / 8;
            new_image.width = tab.width / new_image.bytes_per_pixel;
            new_image.height = tab.height;

            ASSERT(new_image.width > 0, "Sanity check failed", image.name, path.c_str());
            ASSERT(new_image.height > 0, "Sanity check failed", image.name, path.c_str());
            ASSERT(new_image.bytes_per_pixel > 0, "Sanity check failed", image.name, path.c_str());
            new_image.pixels.reset((std::byte*)img.pixbuf_mem_owner.release());
            new_image.mips.emplace_back(new_image.pixels.get(), new_image.byte_size());
          }
        },
        [&](const fastgltf::sources::Array& array) {
          wuffs_aux::DecodeImageCallbacks callbacks;
          wuffs_aux::sync_io::MemoryInput input((const char*)array.bytes.data(), array.bytes.size());
          wuffs_aux::DecodeImageResult img = wuffs_aux::DecodeImage(callbacks, input);

          ASSERT(img.error_message.empty(), img.error_message);
          ASSERT(img.pixbuf.pixcfg.is_valid(), "Something is wrong with the image (idk :) )");

          wuffs_base__table_u8 tab = img.pixbuf.plane(0);
          wuffs_base__pixel_format format = img.pixbuf.pixel_format();

          ASSERT(format.bits_per_pixel() % 8 == 0,
              "Image format bits_per_pixel % 8 != 0. "
              "To handle such cases you'd need to change public API from byte_size to bit_size");

          new_image.bytes_per_pixel = format.bits_per_pixel() / 8;
          new_image.width = tab.width / new_image.bytes_per_pixel;
          new_image.height = tab.height;

          ASSERT(new_image.width > 0, "Sanity check failed", image.name);
          ASSERT(new_image.height > 0, "Sanity check failed", image.name);
          ASSERT(new_image.bytes_per_pixel > 0, "Sanity check failed", image.name);

          new_image.pixels.reset((std::byte*)img.pixbuf_mem_owner.release());
          new_image.mips.emplace_back((std::byte*)tab.ptr, new_image.byte_size());
        },
        [&](const fastgltf::sources::Vector& vector) {
          wuffs_aux::DecodeImageCallbacks callbacks;
          wuffs_aux::sync_io::MemoryInput input((const char*)vector.bytes.data(), vector.bytes.size());
          wuffs_aux::DecodeImageResult img = wuffs_aux::DecodeImage(callbacks, input);

          ASSERT(img.error_message.empty(), img.error_message);
          ASSERT(img.pixbuf.pixcfg.is_valid(), "Something is wrong with the image (idk :) )");

          wuffs_base__table_u8 tab = img.pixbuf.plane(0);
          wuffs_base__pixel_format format = img.pixbuf.pixel_format();

          ASSERT(format.bits_per_pixel() % 8 == 0,
              "Image format bits_per_pixel % 8 != 0. "
              "To handle such cases you'd need to change public API from byte_size to bit_size");

          new_image.bytes_per_pixel = format.bits_per_pixel() / 8;
          new_image.width = tab.width / new_image.bytes_per_pixel;
          new_image.height = tab.height;

          ASSERT(new_image.width > 0, "Sanity check failed", image.name);
          ASSERT(new_image.height > 0, "Sanity check failed", image.name);
          ASSERT(new_image.bytes_per_pixel > 0, "Sanity check failed", image.name);

          new_image.pixels.reset((std::byte*)img.pixbuf_mem_owner.release());
          new_image.mips.emplace_back((std::byte*)tab.ptr, new_image.byte_size());
        },
        [&](const fastgltf::sources::BufferView& view) {
          auto& bufferView = asset.bufferViews[view.bufferViewIndex];
          auto& buffer = asset.buffers[bufferView.bufferIndex];

          std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                         // specify LoadExternalBuffers, meaning all buffers
                                         // are already loaded into a vector.
            [](auto& arg) { ASSERT(false, "Try to process image from buffer view but not from RAM (should be illegal because of LoadExternalBuffers)"); },
            [&](fastgltf::sources::Vector& vector) {
              wuffs_aux::DecodeImageCallbacks callbacks;
              wuffs_aux::sync_io::MemoryInput input((const char*)vector.bytes.data() + bufferView.byteOffset, bufferView.byteLength);
              wuffs_aux::DecodeImageResult img = wuffs_aux::DecodeImage(callbacks, input);

              ASSERT(img.error_message.empty(), img.error_message);
              ASSERT(img.pixbuf.pixcfg.is_valid(), "Something is wrong with the image (idk :) )");

              wuffs_base__table_u8 tab = img.pixbuf.plane(0);
              wuffs_base__pixel_format format = img.pixbuf.pixel_format();

              ASSERT(format.bits_per_pixel() % 8 == 0,
                  "Image format bits_per_pixel % 8 != 0. "
                  "To handle such cases you'd need to change public API from byte_size to bit_size");

              new_image.bytes_per_pixel = format.bits_per_pixel() / 8;
              new_image.width = tab.width / new_image.bytes_per_pixel;
              new_image.height = tab.height;

              ASSERT(new_image.width > 0, "Sanity check failed", image.name);
              ASSERT(new_image.height > 0, "Sanity check failed", image.name);
              ASSERT(new_image.bytes_per_pixel > 0, "Sanity check failed", image.name);

              new_image.pixels.reset((std::byte*)img.pixbuf_mem_owner.release());
              new_image.mips.emplace_back((std::byte*)tab.ptr, new_image.byte_size());
            }
          }, buffer.data);
        },
      },
      image.data);

    if (new_image.format == vk::Format()) {
      switch (new_image.bytes_per_pixel) {
        case 1:
          if (!(options & Options::Allow1ComponentImages)) {
            MR_INFO("Resizing an image from 1-component to 2-component. Consider doing it offline");
            resize_image(new_image, 1, 1, 2);
          }
          else {
            new_image.format = vk::Format::eR8Srgb;
            break;
          }
        case 2:
          if (!(options & Options::Allow2ComponentImages)) {
            MR_INFO("Resizing an image from 2-component to 3-component. Consider doing it offline");
            resize_image(new_image, 2, 1, 3);
          }
          else {
            new_image.format = vk::Format::eR8G8Srgb;
            break;
          }
        case 3:
          if (!(options & Options::Allow3ComponentImages)) {
            MR_INFO("Resizing an image from 3-component to 4-component. Consider doing it offline");
            resize_image(new_image, 3, 1, 4);
          }
          else {
            new_image.format = vk::Format::eR8G8B8Srgb;
            break;
          }
        case 4:
          if (!(options & Options::Allow4ComponentImages)) {
            MR_ERROR("Disallowing 4-component images makes lossless import impossible. Transfer your images to 3-components (or less) offline!");
          }
          else {
            new_image.format = vk::Format::eR8G8B8A8Srgb;
            break;
          }
        default:
          ASSERT(false, "Failed to determine number of image components", new_image.bytes_per_pixel);
          break;
      }
    }

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
      Options options,
      fastgltf::Asset &asset,
      TextureType type,
      const fastgltf::TextureInfo &texinfo)
  {
    fastgltf::Texture &tex = asset.textures[texinfo.textureIndex];

    size_t img_idx = ~0z;

    if (tex.imageIndex.has_value()) {
      img_idx = tex.imageIndex.value();
    }
    if (tex.ddsImageIndex.has_value() && (!(options & Options::PreferUncompressed) || img_idx == ~0z)) {
      img_idx = tex.ddsImageIndex.value();
    }
    if (img_idx == ~0z) {
      return std::unexpected("Texture is in unsupported format");
    }

    fastgltf::Image &img = asset.images[img_idx];
    std::optional<ImageData> img_data_opt = get_image_from_gltf(directory, options, asset, img);
    ASSERT(img_data_opt.has_value(), "Unable to load image");

    return TextureData(
      std::move(img_data_opt.value()),
      type,
      {}
    );
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
      fastgltf::Asset *asset,
      Options options) {
    ASSERT(asset);

    std::vector<MaterialData> materials;
    materials.resize(asset->materials.size());

    tbb::parallel_for(0uz, asset->materials.size(),
      [&asset, &materials, &directory, &options] (size_t i) {
        fastgltf::Material &src = asset->materials[i];
        MaterialData       &dst = materials[i];

        dst.constants.base_color_factor = color_from_nvec4(src.pbrData.baseColorFactor);
        dst.constants.roughness_factor = src.pbrData.roughnessFactor;
        dst.constants.metallic_factor = src.pbrData.metallicFactor;
        dst.constants.emissive_color = color_from_nvec3(src.emissiveFactor);
        dst.constants.normal_map_intensity = 1;
        dst.constants.emissive_strength = src.emissiveStrength;

        if (src.pbrData.baseColorTexture.has_value()) {
          auto exp = get_texture_from_gltf(directory, options, *asset, TextureType::BaseColor, src.pbrData.baseColorTexture.value());
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading Base Color texture - ", exp.error());
          }
        }
        else if (src.specularGlossiness.get() && src.specularGlossiness->diffuseTexture.has_value()) {
          auto exp = get_texture_from_gltf(directory, options, *asset, TextureType::BaseColor, src.specularGlossiness->diffuseTexture.value());
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading Base Color texture - ", exp.error());
          }
        }

        if (src.normalTexture.has_value()) {
          auto exp = get_texture_from_gltf(directory, options, *asset, TextureType::NormalMap, src.normalTexture.value());
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
            options,
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
            options,
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
            auto exp = get_texture_from_gltf(directory, options, *asset, TextureType::OcclusionMap, src.occlusionTexture.value());
            if (exp.has_value()) {
              dst.textures.emplace_back(std::move(exp.value()));
            }
            else {
              MR_ERROR("Loading Occlusion texture - ", exp.error());
            }
          }
        }
        else if (src.specularGlossiness.get() && src.specularGlossiness->specularGlossinessTexture.has_value()) {
          auto exp = get_texture_from_gltf(
            directory,
            options,
            *asset,
            TextureType::RoughnessMetallic,
            src.specularGlossiness->specularGlossinessTexture.value()
          );
          if (exp.has_value()) {
            dst.textures.emplace_back(std::move(exp.value()));
          }
          else {
            MR_ERROR("Loading Specular Glossiness texture - ", exp.error());
          }
        }

        if (src.emissiveTexture.has_value()) {
          auto exp = get_texture_from_gltf(directory, options, *asset, TextureType::EmissiveColor, src.emissiveTexture.value());
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
  std::optional<Model> load(std::filesystem::path path, Options options) {
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
      graph, tbb::flow::unlimited, [&res, &dir, &options](fastgltf::Asset* asset) {
        res.materials = get_materials_from_asset(dir, asset, options);
      }
    };
    materials_load.try_put(&asset.value());

    graph.wait_for_all();

    return res;
  }
  }
}
