cmake_minimum_required(VERSION 3.27)

set(MR_IMPORTER_PROJECT_NAME mr-importer)
set(MR_IMPORTER_LIB_NAME mr-importer-lib)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MR_IMPORTER_BUILD_TESTS    "Whether tests are built" OFF)
option(MR_IMPORTER_BUILD_EXAMPLES "Whether examples are built" OFF)

project(
  ${MR_IMPORTER_PROJECT_NAME}
  VERSION 1.0.6
  LANGUAGES CXX
)

include(cmake/deps.cmake)

add_library(${MR_IMPORTER_LIB_NAME}
    include/mr-importer/def.hpp
    include/mr-importer/assets.hpp
    include/mr-importer/importer.hpp
    include/mr-importer/loader.hpp
    include/mr-importer/optimizer.hpp
    include/mr-importer/compiler.hpp

    src/mr-importer/assets.cpp
    src/mr-importer/loader.cpp
    src/mr-importer/optimizer.cpp
    src/mr-importer/compiler.cpp
    src/mr-importer/pch.hpp
)
target_compile_features(${MR_IMPORTER_LIB_NAME} PUBLIC cxx_std_23)
target_link_libraries(${MR_IMPORTER_LIB_NAME} PUBLIC ${MR_IMPORTER_PUBLIC_DEPS})
target_link_libraries(${MR_IMPORTER_LIB_NAME} PRIVATE ${MR_IMPORTER_PRIVATE_DEPS})
target_include_directories(${MR_IMPORTER_LIB_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_precompile_headers(${MR_IMPORTER_LIB_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/mr-importer/def.hpp>
    $<INSTALL_INTERFACE:include/mr-importer/def.hpp>

  PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/mr-importer/pch.hpp>
    $<INSTALL_INTERFACE:src/mr-importer/pch.hpp>
)

if (MR_IMPORTER_BUILD_TESTS)
  add_executable(
    mr-importer-tests
    tests/main.cpp
  )
  target_link_libraries(mr-importer-tests PUBLIC mr-importer-lib gtest_main gtest)
endif()

if (MR_IMPORTER_BUILD_EXAMPLES)
  CPMAddPackage("gh:cone-forest/polyscope#master")

  add_executable(
    mr-importer-mesh-example
    examples/rendertest.cpp
  )
  target_link_libraries(mr-importer-mesh-example PUBLIC mr-importer-lib polyscope)

  add_executable(
    mr-importer-shader-example
    examples/shadercompiletest.cpp
  )
  target_link_libraries(mr-importer-shader-example PUBLIC mr-importer-lib)
endif()

install(TARGETS ${MR_IMPORTER_LIB_NAME}
    EXPORT mr-importer-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)
install(EXPORT mr-importer-targets
    FILE mr-importer-config.cmake
    NAMESPACE mr-importer::
    DESTINATION lib/cmake/mr-importer
)
