#pragma once

#include <array>
#include <cmath>
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

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

#include <mr-manager/manager.hpp>
#include <mr-contractor/contractor.hpp>

namespace mr {
  template <typename Ret, typename... Args> struct TaskPrototypeBuilder;

  template <typename Ret, typename... Args>
  auto get_task_prototype() {
    return std::ref(TaskPrototypeBuilder<Ret, Args...>::create());
  }

  template <typename ResultT, typename ...Args>
  auto make_task(Args ...args) {
    if constexpr (sizeof...(args) > 1) {
      return mr::apply(
        get_task_prototype<ResultT, Args...>(),
        std::forward_as_tuple<Args...>(args...)
      );
    }
    else {
      return mr::apply(
        get_task_prototype<ResultT, Args...>(),
        args...
      );
    }
  }

inline namespace importer {
}
}
