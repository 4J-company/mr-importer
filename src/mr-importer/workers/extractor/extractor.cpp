#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "workers/extractor/extractor.hpp"

namespace mr::detail {
  template <typename T>
  inline std::optional<AttribData<T>>
  loadAttribute(const fastgltf::Asset &asset, const fastgltf::Primitive &prim,
                std::string_view name)
  {
    const auto &it = prim.findAttribute(name);
    if (it == prim.attributes.cend()) {
      return std::nullopt;
    }

    const auto &accessor = asset.accessors[it->accessorIndex];
    if (!accessor.bufferViewIndex) {
      return std::nullopt;
    }

    const auto componentType = accessor.componentType;
    const auto attributeType = accessor.type;
    const auto numElements = accessor.count;

    std::vector<T> data;
    data.resize(numElements);

    // fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset, accessor, data.data());

    if (attributeType == fastgltf::AccessorType::Vec3) {
      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
        asset, accessor, [&](fastgltf::math::fvec3 pos, std::size_t idx) {
          data[idx][0] = pos.x();
          data[idx][1] = pos.y();
          data[idx][2] = pos.z();
        });
    }
    else if (attributeType == fastgltf::AccessorType::Vec2) {
      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
        asset, accessor, [&](fastgltf::math::fvec2 pos, std::size_t idx) {
          data[idx][0] = pos.x();
          data[idx][1] = pos.y();
        });
    }

    return std::move(data);
  }

  [[nodiscard]] mr::VertexAttribsMap
  extractVertexData(const fastgltf::Asset &asset,
                    const fastgltf::Primitive &prim);
  [[nodiscard]] std::optional<ImageData>
  loadImage(const fastgltf::Asset &asset, const fastgltf::Image &image);
  [[nodiscard]] std::optional<SamplerData>
  loadSampler(const fastgltf::Asset &asset, const fastgltf::Sampler &sampler);
  [[nodiscard]] std::optional<std::pair<mr::ImageData, mr::SamplerData>>
  loadTexture(const fastgltf::Asset &asset, const fastgltf::Texture &tex);
  [[nodiscard]] mr::MaterialData
  loadMaterial(const fastgltf::Asset &asset,
               const fastgltf::Material &material);
} // namespace mr::detail

[[nodiscard]] mr::VertexAttribsMap
mr::detail::extractVertexData(const fastgltf::Asset& asset,
                              const fastgltf::Primitive& prim)
{
  static constexpr std::array texcoords_names {
    "TEXCOORD_0",
    "TEXCOORD_1",
    "TEXCOORD_2",
    "TEXCOORD_3",
    "TEXCOORD_4",
    "TEXCOORD_5",
    "TEXCOORD_6",
    "TEXCOORD_7",
  };

  VertexAttribsMap attributes;

  const auto& accessor = asset.accessors[prim.indicesAccessor.value()];

  attributes.positions =
    detail::loadAttribute<mra::Vec3f>(asset, prim, "POSITION").value();
  attributes.normals =
    detail::loadAttribute<mra::Vec3f>(asset, prim, "NORMAL").value();

  attributes.indices.resize(accessor.count);
  fastgltf::copyFromAccessor<std::uint32_t>(
    asset, accessor, attributes.indices.data());

  // load attributes
  for (auto name : texcoords_names) {
    auto opt = detail::loadAttribute<mra::Vec2f>(asset, prim, name);
    if (!opt) {
      break;
    }
    attributes.texcoords.emplace_back(std::move(opt.value()));
  }

  return attributes;
}

[[nodiscard]] std::optional<mr::ImageData>
mr::detail::loadImage(const fastgltf::Asset& asset,

                      const fastgltf::Image& image)
{
  auto getLevelCount = [](int width, int height) -> int {
    using namespace std;
    return 1 + floor(log2(max(width, height)));
  };

  ImageData texture;
  std::visit(
    fastgltf::visitor {
      [](auto& arg) {},

      [&](fastgltf::sources::URI& filePath) {
        assert(filePath.fileByteOffset ==
               0);                // We don't support offsets with stbi.
        assert(filePath.uri
                 .isLocalPath()); // We're only capable of loading local files.
        int width, height, nrChannels;

        const std::string path(filePath.uri.path().begin(),
                               filePath.uri.path().end()); // Thanks C++.
        unsigned char* data =
          stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
        texture.data.resize(width * height * nrChannels);
        std::copy(
          data, data + width * height * nrChannels, texture.data.data());
        texture.width = width;
        texture.height = height;
        texture.componentCount = nrChannels;
        stbi_image_free(data);
      },

      [&](fastgltf::sources::Array& vector) {
        int width, height, nrChannels;
        unsigned char* data = stbi_load_from_memory(
          reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
          static_cast<int>(vector.bytes.size()),
          &width,
          &height,
          &nrChannels,
          4);
        texture.data.resize(width * height * nrChannels);
        std::copy(
          data, data + width * height * nrChannels, texture.data.data());
        texture.width = width;
        texture.height = height;
        texture.componentCount = nrChannels;
        stbi_image_free(data);
      },

      [&](fastgltf::sources::BufferView& view) {
        auto& bufferView = asset.bufferViews[view.bufferViewIndex];
        auto& buffer = asset.buffers[bufferView.bufferIndex];
        std::visit(
          fastgltf::visitor {[](auto& arg) {},

                             [&](fastgltf::sources::Array& vector) {
                               int width, height, nrChannels;
                               unsigned char* data = stbi_load_from_memory(
                                 reinterpret_cast<const stbi_uc*>(
                                   vector.bytes.data() + bufferView.byteOffset),
                                 static_cast<int>(bufferView.byteLength),
                                 &width,
                                 &height,
                                 &nrChannels,
                                 4);
                               texture.data.resize(width * height * nrChannels);
                               std::copy(data,
                                         data + width * height * nrChannels,
                                         texture.data.data());
                               texture.width = width;
                               texture.height = height;
                               texture.componentCount = nrChannels;
                               stbi_image_free(data);
                             }},
          buffer.data);
      },
    },
    image.data);

  return texture;
}

[[nodiscard]] std::optional<mr::SamplerData>
mr::detail::loadSampler(const fastgltf::Asset &asset, const fastgltf::Sampler &sampler) {
  return sampler;
}

[[nodiscard]] std::optional<std::pair<mr::ImageData, mr::SamplerData>>
mr::detail::loadTexture(const fastgltf::Asset &asset, const fastgltf::Texture &tex) {
  if (!tex.imageIndex || !tex.samplerIndex) {
    return std::nullopt;
  }

  const fastgltf::Image& image = asset.images[tex.imageIndex.value()];
  std::optional<ImageData> imageData = loadImage(asset, image);

  if (!imageData) {
    return std::nullopt;
  }

  const fastgltf::Sampler& sampler = asset.samplers[tex.samplerIndex.value()];
  std::optional<SamplerData> samplerData = loadSampler(asset, sampler);

  if (!samplerData) {
    return std::nullopt;
  }

  return std::pair{std::move(imageData.value()), std::move(samplerData.value())};
}

[[nodiscard]] mr::MaterialData
mr::detail::loadMaterial(const fastgltf::Asset& asset,
                         const fastgltf::Material& material)
{
  mr::MaterialData res;

  res.factors.baseColor = {
    material.pbrData.baseColorFactor.x(),
    material.pbrData.baseColorFactor.y(),
    material.pbrData.baseColorFactor.z(),
  };
  res.factors.occlusionMetallicRoughness = {
    1,
    material.pbrData.metallicFactor,
    material.pbrData.roughnessFactor,
  };
  res.factors.emissive = {
    material.emissiveFactor.x(),
    material.emissiveFactor.y(),
    material.emissiveFactor.z(),
  };

  if (material.pbrData.baseColorTexture) {
    res.textures.baseColor = {
      (int)material.pbrData.baseColorTexture.value().textureIndex,
      (int)material.pbrData.baseColorTexture.value().texCoordIndex,
    };
  }
  if (material.pbrData.metallicRoughnessTexture) {
    res.textures.occlusionMetallicRoughness = {
      (int)material.pbrData.metallicRoughnessTexture.value().textureIndex,
      (int)material.pbrData.metallicRoughnessTexture.value().texCoordIndex,
    };
  }
  if (material.emissiveTexture) {
    res.textures.emissive = {
      (int)material.emissiveTexture.value().textureIndex,
      (int)material.emissiveTexture.value().texCoordIndex,
    };
  }

  return res;
}

mr::VertexAttribsMap mr::Extractor::addTask(const fastgltf::Asset& asset,
                                            const fastgltf::Primitive& prim)
{
  return mr::detail::extractVertexData(asset, prim);
}

std::optional<mr::ImageData>
mr::Extractor::addTask(const fastgltf::Asset& asset, const fastgltf::Image& img)
{
  return mr::detail::loadImage(asset, img);
}

mr::MaterialData mr::Extractor::addTask(const fastgltf::Asset& asset,
                                        const fastgltf::Material& mtl)
{
  return mr::detail::loadMaterial(asset, mtl);
}
