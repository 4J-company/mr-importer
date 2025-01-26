#pragma once

/*
 * STL related code
 */

#include <array>
#include <filesystem>
#include <map>

namespace std {
  namespace fs = filesystem;
}

using namespace std::literals;

/*
 * fastgltf related code
 */

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

namespace fg = fastgltf;

/*
 * meshoptimizer related code
 */

#include <meshoptimizer.h>

/*
 * vtll related code
 */

#include "vtll.hpp"

/*
 * mr related code
 */


namespace mr::aligned {
};

namespace mra = mr::aligned;

namespace mr {
  template<class... Ts>
    struct Overloads : Ts... { using Ts::operator()...; };

  namespace aligned {
    using Vec2f = std::array<float, 2>;
    using Vec3f = std::array<float, 3>;
    using Vec4f = std::array<float, 4>;
  } // namespace aligned

  template <typename T = mr::aligned::Vec3f> using AttribData = std::vector<T>;

  struct ImageData {
      std::vector<char> data {};
      int width = 0;
      int height = 0;
      int componentCount = 0;
  };

  using SamplerData = fastgltf::Sampler;

  struct TextureData {
      int textureIndex = 0;
      int textureCoordIndex = 0;
  };

  struct MaterialData {
      struct {
          TextureData baseColor;
          TextureData occlusionMetallicRoughness;
          std::optional<TextureData> emissive;
          std::optional<TextureData> normal;
      } textures;

      struct {
          mra::Vec3f baseColor {1, 1, 1};
          mra::Vec3f occlusionMetallicRoughness {1, 1, 1};
          mra::Vec3f emissive {1, 1, 1};
      } factors;
  };

  struct VertexAttribsMap
      : std::map<std::string, std::optional<AttribData<mra::Vec3f>>> {
      AttribData<> positions;
      AttribData<> normals;
      std::vector<std::uint32_t> indices;
      std::vector<std::vector<mra::Vec2f>> texcoords;
  };
} // namespace mr

#include "log.hpp"
