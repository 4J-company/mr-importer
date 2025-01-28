cmake_minimum_required(VERSION 3.27)
project(mr-importer)

set(CMAKE_CXX_STANDARD 23)

include(cmake/deps.cmake)

add_library(mr-importer-lib
  src/mr-importer/def.hpp

  src/mr-importer/workers/composer.hpp

  src/mr-importer/workers/extractor/extractor.hpp
  src/mr-importer/workers/extractor/extractor.cpp

  src/mr-importer/workers/compiler/compiler.hpp
  src/mr-importer/workers/compiler/compiler.cpp

  src/mr-importer/workers/optimizer/optimizer.hpp
  src/mr-importer/workers/optimizer/optimizer.cpp

  src/mr-importer/workers/parser.hpp
  src/mr-importer/workers/uploader.hpp

  src/mr-importer/manager/manager.hpp
  src/mr-importer/manager/resource.hpp

  src/mr-importer/pipe/pipe.cpp
  src/mr-importer/pipe/pipe.hpp
)
target_compile_features(mr-importer-lib INTERFACE cxx_std_23)
target_link_libraries(mr-importer-lib PUBLIC ${MR_IMPORTER_DEPS})
target_include_directories(mr-importer-lib PUBLIC src src/mr-importer)

add_executable(
  mr-importer-bench-seq
  bench/seq-main.cpp
)
target_link_libraries(mr-importer-bench-seq PUBLIC mr-importer-lib)

add_executable(
  mr-importer-bench-par
  bench/par-main.cpp
)
target_link_libraries(mr-importer-bench-par PUBLIC mr-importer-lib)

if (FALSE)
add_executable(
  mr-importer-bench-mgr
  bench/mgr-main.cpp
)
target_link_libraries(mr-importer-bench-mgr PUBLIC mr-importer-lib)
endif()

add_executable(
  mr-importer-bench-exe
  bench/exe-main.cpp
)
target_link_libraries(mr-importer-bench-exe PUBLIC mr-importer-lib)

add_executable(
  mr-importer-bench-final
  bench/final.cpp
)
target_link_libraries(mr-importer-bench-final PUBLIC mr-importer-lib)
