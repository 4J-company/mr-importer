#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include <mr-utils/assert.hpp>
#include <fstream> // TODO: place it insice <mr-utils/misc.hpp>
#include <mr-utils/misc.hpp>

#include <mr-math/math.hpp>

namespace mr {
inline namespace importer {
}
} // namespace mr
