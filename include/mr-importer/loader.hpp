#pragma once

#include "assets.hpp"

namespace mr {
inline namespace importer {
  std::optional<Asset> load(std::filesystem::path path);
}
} // namespace mr
