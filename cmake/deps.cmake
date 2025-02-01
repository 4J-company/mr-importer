file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.40.2/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=c8cdc32c03816538ce22781ed72964dc864b2a34a310d3b7104812a5ca2d835d
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

CPMAddPackage("gh:SpartanJ/efsw#master")
CPMAddPackage("gh:spnda/fastgltf#main")
CPMAddPackage("gh:zeux/meshoptimizer#master")
CPMAddPackage(
  NAME work_contract
  GITHUB_REPOSITORY buildingcpp/work_contract
  GIT_TAG main
  OPTIONS
    "WORK_CONTRACT_BUILD_BENCHMARK OFF"
)
# CPMAddPackage("gh:shader-slang/slang#master")

if (NOT TARGET libstb-image)
  # download a single file from stb
  file(
    DOWNLOAD
    https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    ${CMAKE_CURRENT_BINARY_DIR}/_deps/stb-src/stb/stb_image.h
    EXPECTED_HASH SHA256=594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
  )
  add_library(libstb-image INTERFACE "")
  target_include_directories(libstb-image INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/_deps/stb-src/)
endif()

find_package(TBB REQUIRED tbb)

set(MR_IMPORTER_DEPS fastgltf::fastgltf meshoptimizer tbb libstb-image work_contract efsw)
