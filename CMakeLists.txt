cmake_minimum_required(VERSION 3.27)
project(mr-importer)

include(cmake/deps.cmake)

add_library(mr-importer-lib
  src/mr-importer/def.hpp

  src/mr-importer/workers/composer.hpp
  src/mr-importer/workers/extractor.hpp
  src/mr-importer/workers/optimizer.hpp
  src/mr-importer/workers/parser.hpp
  src/mr-importer/workers/uploader.hpp

  src/mr-importer/manager/manager.hpp
  src/mr-importer/manager/resource.hpp
  src/mr-importer/manager/pipeline.hpp

  src/mr-importer/workers/extractor.cpp
)
target_compile_features(mr-importer-lib PUBLIC cxx_std_23)
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

add_executable(
  mr-importer-bench-mgr
  bench/mgr-main.cpp
)
target_link_libraries(mr-importer-bench-mgr PUBLIC mr-importer-lib)

add_executable(
  mr-importer-bench-final
  bench/final.cpp
)
target_link_libraries(mr-importer-bench-final PUBLIC mr-importer-lib)
