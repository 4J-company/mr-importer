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
    /** \brief Run mesh optimization passes and generate LODs. */
    None = 0u,

    /** \brief Run mesh optimization passes and generate LODs. */
    OptimizeMeshes = 1 << 0,

    /** \brief Prefer BCn over non-compressed formats. */
    PreferUncompressed = 1 << 1,

    /** \brief Allow 1-component images. */
    Allow1ComponentImages = 1 << 2,
    /** \brief Allow 2-component images. */
    Allow2ComponentImages = 1 << 3,
    /** \brief Allow 3-component images. */
    Allow3ComponentImages = 1 << 4,
    /** \brief Allow 4-component images. */
    Allow4ComponentImages = 1 << 5,

    /** \brief Enable all available import behaviors. */
    All = ~None,
  };
} // namespace importer
} // namespace mr
