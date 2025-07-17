#pragma once

#include <array>
#include <cmath>
#include <execution>
#include <filesystem>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <span>

#include <fmt/core.h>

#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include <meshoptimizer.h>

#include <mr-manager/manager.hpp>
#include <mr-math/math.hpp>
#include <mr-utils/log.hpp>
#include <mr-utils/assert.hpp>

namespace mr {
inline namespace importer {
}
}
