#pragma once

// Include the lean public header first
#include "mr-importer/def.hpp"

// Additional standard library headers for implementation
#include <cmath>
#include <execution>
#include <expected>
#include <functional>
#include <tuple>
#include <utility>

// Third-party implementation dependencies
#define FASTGLTF_ENABLE_DEPRECATED_EXT 1
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <meshoptimizer.h>

// Internal model-renderer dependencies
#include <mr-math/math.hpp>
#include <mr-utils/log.hpp>

#include <tbb/concurrent_vector.h>
#include <tbb/flow_graph.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>

#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <tracy/Tracy.hpp>
