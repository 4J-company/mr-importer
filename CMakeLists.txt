cmake_minimum_required(VERSION 3.27)
project(mr-importer)

set(BUILD_SHARED_LIBS OFF)
option(MR_IMPORTER_BUILD_TESTS    "Whether tests are built" OFF)
option(MR_IMPORTER_BUILD_EXAMPLES "Whether examples are built" OFF)

include(cmake/deps.cmake)

add_library(mr-importer-lib
  include/mr-importer/def.hpp
  include/mr-importer/assets.hpp
  include/mr-importer/assets.cpp
  include/mr-importer/importer.hpp
  include/mr-importer/loader.hpp
  include/mr-importer/loader.cpp
  include/mr-importer/optimizer.hpp
  include/mr-importer/optimizer.cpp
)
target_compile_features(mr-importer-lib PUBLIC cxx_std_23)
target_link_libraries(mr-importer-lib PUBLIC ${MR_IMPORTER_DEPS})
target_include_directories(mr-importer-lib PUBLIC include)
target_precompile_headers(mr-importer-lib PUBLIC include/mr-importer/def.hpp)

if (MR_IMPORTER_BUILD_TESTS)
  add_executable(
    mr-importer-tests
    tests/main.cpp
  )
  target_link_libraries(mr-importer-tests PUBLIC mr-importer-lib gtest_main gtest)
endif()

if (MR_IMPORTER_BUILD_EXAMPLES)
  add_executable(
    mr-importer-example
    examples/rendertest.cpp
  )
  target_link_libraries(mr-importer-example PUBLIC mr-importer-lib polyscope)
endif()
