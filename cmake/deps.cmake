file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# public dependencies
find_package(glm REQUIRED)
find_package(mr-math REQUIRED)
find_package(mr-utils REQUIRED)
find_package(Vulkan REQUIRED)
set(MR_IMPORTER_PUBLIC_DEPS
  glm::glm
  mr-math::mr-math
  mr-utils::mr-utils
  Vulkan::Headers
  OpenSubdiv::osdgpu_static
)

# private dependencies
find_package(meshoptimizer REQUIRED)
find_package(fastgltf REQUIRED)
find_package(slang REQUIRED)
find_package(TBB REQUIRED)
find_package(Ktx REQUIRED)
find_package(Tracy REQUIRED)
find_package(draco REQUIRED)
find_package(pxr REQUIRED)
find_package(OpenSubdiv REQUIRED)

# OpenUSD discovers file formats (Sdf, Ar, …) via PlugRegistry. Conan layouts
# vary; CMakeDeps may expose openusd_* or pxr_* package folders. conanfile.py
# also sets MR_IMPORTER_PXR_USD_PLUGIN_ROOT via CMakeToolchain when possible.
if(NOT MR_IMPORTER_PXR_USD_PLUGIN_ROOT)
  foreach(
    _root
    "${openusd_PACKAGE_FOLDER_RELEASE}"
    "${openusd_PACKAGE_FOLDER_DEBUG}"
    "${pxr_PACKAGE_FOLDER_RELEASE}"
    "${pxr_PACKAGE_FOLDER_DEBUG}")
    if(_root STREQUAL "")
      continue()
    endif()
    if(EXISTS "${_root}/lib/usd/sdf/resources/plugInfo.json")
      set(MR_IMPORTER_PXR_USD_PLUGIN_ROOT "${_root}/lib/usd")
      break()
    endif()
  endforeach()
endif()
CPMAddPackage("gh:spnda/dds_image#main")
CPMAddPackage(
  NAME wuffs
  GITHUB_REPOSITORY google/wuffs-mirror-release-c
  GIT_TAG main
  DOWNLOAD_ONLY
)
if (${wuffs_ADDED})
  add_library(wuffs INTERFACE "")
  target_include_directories(wuffs INTERFACE ${wuffs_SOURCE_DIR}/release/c)
endif()

set(MR_IMPORTER_PRIVATE_DEPS
  meshoptimizer::meshoptimizer
  fastgltf::fastgltf
  slang::slang
  TBB::tbb
  KTX::ktx
  draco::draco
  openusd::openusd
  dds_image
  wuffs
  Tracy::TracyClient
)
