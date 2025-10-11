file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# public dependencies
find_package(glm REQUIRED)
find_package(slang REQUIRED)
find_package(mr-math REQUIRED)
find_package(mr-utils REQUIRED)
find_package(mr-manager REQUIRED)
find_package(VulkanHeaders REQUIRED)
CPMAddPackage("gh:bemanproject/inplace_vector#b81a3c7")
set(MR_IMPORTER_PUBLIC_DEPS
  glm::glm
  slang::slang
  mr-math::mr-math
  mr-utils::mr-utils
  mr-manager::mr-manager
  vulkan-headers::vulkan-headers
  beman.inplace_vector
)

# private dependencies
find_package(fmt REQUIRED)
find_package(efsw REQUIRED)
find_package(meshoptimizer REQUIRED)
find_package(fastgltf REQUIRED)
find_package(stb REQUIRED)
find_package(TBB REQUIRED)
CPMAddPackage("gh:spnda/dds_image#main")

set(MR_IMPORTER_PRIVATE_DEPS
  meshoptimizer::meshoptimizer
  fastgltf::fastgltf
  efsw::efsw
  stb::stb
  fmt::fmt
  onetbb::onetbb
  dds_image
)
