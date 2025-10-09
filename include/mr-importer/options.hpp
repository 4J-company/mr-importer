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
    /** \brief Enable all available import behaviors. */
    All = ~None,
  };
} // namespace importer
} // namespace mr
