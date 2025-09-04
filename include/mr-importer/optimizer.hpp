#pragma once

/**
 * \file optimizer.hpp
 * \brief Public API for mesh optimization and LOD generation.
 */

#include "assets.hpp"

namespace mr {
inline namespace importer {
  /**
   * \brief Optimize mesh for GPU rendering and generate LODs.
   *
   * Performs vertex cache optimization, overdraw reduction and vertex fetch
   * remapping, then builds shadow-optimized index buffers and multiple LODs.
   * \param mesh Input mesh data.
   * \return Optimized mesh with at least one LOD populated.
   */
  Mesh optimize(Mesh mesh);
}
} // namespace mr
