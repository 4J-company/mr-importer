#pragma once

#include "def.hpp"
#include "assets.hpp"
#include "compiler.hpp"
#include "loader.hpp"
#include "optimizer.hpp"
#include "options.hpp"

namespace mr {
inline namespace importer {
  inline std::optional<Asset> import(const std::filesystem::path& path, uint32_t options = Options::All)
  {
    std::optional<Asset> asset = load(path);

    if (!asset) {
      return std::nullopt;
    }
  
    if (options & Options::OptimizeMeshes) {
      for (Mesh& mesh : asset.value().meshes) {
        mesh = mr::optimize(std::move(mesh));
      }
    }
  
    return asset;
  }
} // namespace importer
} // namespace mr
