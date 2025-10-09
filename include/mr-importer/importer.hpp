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
   * \return Imported \ref Model or std::nullopt if loading failed.
   */
  std::optional<Model> import(const std::filesystem::path& path, Options options = Options::All);
} // namespace importer
} // namespace mr
