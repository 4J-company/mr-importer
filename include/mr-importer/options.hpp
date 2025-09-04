#pragma once

#include <cstdint>

namespace mr {
inline namespace importer {
  /** \brief Import options bitmask. */
  enum Options : std::uint32_t {
    /** \brief Run mesh optimization passes and generate LODs. */
    OptimizeMeshes = 1 << 0,
    /** \brief Enable all available import behaviors. */
    All = 1 << 1,
  };
} // namespace importer
} // namespace mr
