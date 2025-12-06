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
  
    /** \brief Load materials */
    LoadMaterials = 1 << 3,
    /** \brief Prefer BCn over non-compressed formats. */
    PreferUncompressed = 1 << 4,
    /** \brief Allow 1-component images. */
    Allow1ComponentImages = 1 << 5,
    /** \brief Allow 2-component images. */
    Allow2ComponentImages = 1 << 6,
    /** \brief Allow 3-component images. */
    Allow3ComponentImages = 1 << 7,
    /** \brief Allow 4-component images. */
    Allow4ComponentImages = 1 << 8,
  
    /** \brief Load attributes */
    LoadMeshAttributes = 1 << 9,

    All = ~None,
  };

  constexpr bool is_enabled(Options options, uint32_t option) noexcept {
    return (options & option) == option;
  }
  constexpr bool is_disabled(Options options, uint32_t option) noexcept {
    return !(options & option);
  }

  static_assert(is_enabled(Options::None, Options::None));
  static_assert(is_disabled(Options::All, Options::None));

  constexpr Options & enable(Options &options, uint32_t option) noexcept {
    return options = Options(options | option);
  }
  constexpr Options & disable(Options &options, uint32_t option) noexcept {
    return options = Options(options & ~option);
  }
} // namespace importer
} // namespace mr
