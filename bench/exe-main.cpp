#include <functional>

#include "mr-importer/pipeline/pipeline.hpp"

int main() {

  /*
  mr::Pipeline<int> pipeline {0};
  pipeline
    .add_step([](int *i) { *i = 1; })
    .add_step([](int *i) { *i += 1; })
    .add_step([](int *i) { *i += 1; })
    .add_step([](int *i) { *i += 1; })
    .add_step([](int *i) { *i *= 10; })
    ;
  pipeline.execute();

  std::print("result: {}\n", *pipeline.object.get());
  */

   auto prot = mr::PipePrototype{
     std::function([](int) -> float { return 30; }),
     std::function([](float) -> int { return 47; }),
     std::function([](int) -> float { return 80; })
    };
   auto conc = prot.on(0);
   conc.schedule();
   std::print("result: {}", conc.result());
}
