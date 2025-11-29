#pragma once

/**
 * \file options.hpp
 * \brief Import configuration flags.
 */

#include <cstdint>

namespace mr {
inline namespace importer {
  /** \brief Import options bitmask. */
  enum Options : std::uint32_t {
    None = 0u,
  
    /** \brief Run mesh geometry and layout optimization passes. */
    OptimizeMeshes = 1 << 0,
    /** \brief Run discrete LOD generation (using meshoptimizer) */
    GenerateDiscreteLODs = 1 << 1,
    /** \brief Generate meshlet division for each discrete LOD (including original mesh) */
    GenerateMeshlets = 1 << 2,
  
    /** \brief Prefer BCn over non-compressed formats. */
    PreferUncompressed = 1 << 3,
  
    /** \brief Allow 1-component images. */
    Allow1ComponentImages = 1 << 4,
    /** \brief Allow 2-component images. */
    Allow2ComponentImages = 1 << 5,
    /** \brief Allow 3-component images. */
    Allow3ComponentImages = 1 << 6,
    /** \brief Allow 4-component images. */
    Allow4ComponentImages = 1 << 7,
  
    All = ~None,
  };
} // namespace importer
} // namespace mr
