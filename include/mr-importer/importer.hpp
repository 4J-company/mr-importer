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
#include "serializer.hpp"
#include "options.hpp"

namespace mr {
inline namespace importer {
  /**
   * \brief High-level import entry point.
   *
   * Loads an asset from disk, optionally optimizes meshes, and returns the result.
   * \param path Path to a source asset (e.g. glTF file).
   * \param options Import behavior flags, see \ref Options.
   * \return Imported \ref Model or std::nullopt if loading failed.
   *
   * USD (\c .usd, \c .usda, \c .usdc, \c .usdz) honors the same \c Options as glTF where
   * applicable: \c LoadMaterials, \c LoadMeshAttributes, image flags (\c PreferUncompressed,
   * \c Allow*ComponentImages) for textures, and post-load \c OptimizeMeshes, \c GenerateDiscreteLODs,
   * \c GenerateMeshlets. OpenUSD plugins must be discoverable (see \c MR_IMPORTER_USD_PLUGIN_ROOT
   * / \c PXR_PLUGINPATH) or \c .usdz and some references can fail to resolve.
   */
  std::optional<Model> import(const std::filesystem::path& path, Options options = Options::All);
} // namespace importer
} // namespace mr
