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
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <meshoptimizer.h>

#include <stb_image.h>

// Internal model-renderer dependencies
#include <mr-manager/manager.hpp>
#include <mr-utils/assert.hpp>
#include <mr-utils/log.hpp>
#include <mr-math/math.hpp>

#include <tbb/flow_graph.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>

