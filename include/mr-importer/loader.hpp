#pragma once

#include <filesystem>
#include "assets.hpp"

namespace mr {
inline namespace importer {
  Asset load(std::filesystem::path path);
}
}
