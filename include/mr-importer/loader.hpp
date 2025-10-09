#pragma once

/**
 * \file loader.hpp
 * \brief Public API to load and convert source assets (e.g., glTF).
 */

#include "assets.hpp"
#include "options.hpp"

namespace mr {
inline namespace importer {
  /**
   * \brief Parse and load a source asset into runtime data structures.
   * \param path Path to the source file (e.g. glTF).
   * \return Loaded \ref Model on success, or std::nullopt on failure.
   */
  std::optional<Model> load(std::filesystem::path path, Options options);
}
} // namespace mr
