#pragma once

#include "assets.hpp"

namespace mr {
inline namespace importer {
  /**
   * \brief Parse and load a source asset into runtime data structures.
   * \param path Path to the source file (e.g. glTF).
   * \return Loaded \ref Asset on success, or std::nullopt on failure.
   */
  std::optional<Asset> load(std::filesystem::path path);
}
} // namespace mr
