#pragma once

#include <array>
#include <cmath>
#include <execution>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <fmt/core.h>
#include <functional>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <meshoptimizer.h>
#include <mr-manager/manager.hpp>
#include <mr-math/math.hpp>
#include <mr-utils/assert.hpp>
#include <mr-utils/log.hpp>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>
#include <span>
#include <stb_image.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace mr {
inline namespace importer {
}
} // namespace mr
