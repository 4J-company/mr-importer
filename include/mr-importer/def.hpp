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

#if __cpp_lib_inplace_vector >= 202406L
#include <inplace_vector>
namespace mr {
inline namespace importer {
  template <typename T, size_t N> using InplaceVector = std::inplace_vector<T, N>;
}
} // namespace mr
#else
#include <beman/inplace_vector/inplace_vector.hpp>
namespace mr {
inline namespace importer {
  template <typename T, size_t N> using InplaceVector = beman::inplace_vector<T, N>;
}
} // namespace mr
#endif
