#pragma once

/**
 * \file importer.hpp
 * \brief High-level import facade that wires loader and optimizer.
 */

#include "def.hpp"
#include "assets.hpp"
#include "compiler.hpp"
#include "loader.hpp"
#include "optimizer.hpp"
#include "options.hpp"

namespace mr {
inline namespace importer {
  /**
   * \brief High-level import entry point.
   *
   * Loads an asset from disk, optionally optimizes meshes, and returns the result.
   * \param path Path to a source asset (e.g. glTF file).
   * \param options Import behavior flags, see \ref Options.
   * \return Imported \ref Asset or std::nullopt if loading failed.
   */
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
