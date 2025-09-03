#pragma once

#include "def.hpp"
#include "assets.hpp"

namespace mr {
inline namespace importer {
  std::optional<Shader> compile(const std::filesystem::path& path);
}
} // namespace mr
