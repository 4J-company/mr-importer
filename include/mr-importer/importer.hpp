#pragma once

#include "def.hpp"
#include "options.hpp"
#include "assets.hpp"
#include "loader.hpp"
#include "optimizer.hpp"
#include "compiler.hpp"

namespace mr {
inline namespace importer {
  inline Asset import(const std::filesystem::path &path, uint32_t options = Options::All) {
    Asset asset = load(path);

    if (options & Options::OptimizeMeshes) {
      for (Mesh& mesh : asset.meshes) {
        mesh = mr::optimize(std::move(mesh));
      }
    }

    return asset;
  }
}
}
