#pragma once

#include "def.hpp"

namespace mr {
  struct Extractor {
      static mr::VertexAttribsMap addTask(const fastgltf::Asset &asset,
                                          const fastgltf::Primitive &prim);
      static std::optional<mr::ImageData> addTask(const fastgltf::Asset &asset,
                                                  const fastgltf::Image &img);
      static mr::MaterialData addTask(const fastgltf::Asset &asset,
                                      const fastgltf::Material &mtl);
  };
} // namespace mr
