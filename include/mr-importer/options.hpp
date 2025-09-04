#pragma once

#include <cstdint>

namespace mr {
inline namespace importer {
  enum Options : std::uint32_t {
    OptimizeMeshes = 1 << 0,
    All = 1 << 1,
  };
} // namespace importer
} // namespace mr
