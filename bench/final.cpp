#include "mr-importer/workers/composer.hpp"
#include "mr-importer/manager/pipeline.hpp"
#include "mr-importer/manager/resource.hpp"
#include "mr-importer/manager/manager.hpp"

using namespace std::literals;

int main() {
  mr::ImportPipeline<int> pipeline {0};
  pipeline
    .add_node([](int *i) { *i = 1; })
    .add_node([](int *i) { *i += 1; })
    .add_node([](int *i) { *i += 1; })
    .add_node([](int *i) { *i += 1; })
    .add_node([](int *i) { *i *= 10; })
    ;
  pipeline.execute();

  std::print("result: {}\n", *pipeline.object.get());
}
