#include <functional>

#include "mr-importer/pipe/pipe.hpp"

int main() {
   auto prot = mr::PipePrototype{
     std::function([](int) -> float { std::println("Stage #1 done"); return 30; }),
     std::function([](float) -> int { std::println("Stage #2 done"); return 47; }),
     std::function([](int) -> float { std::println("Stage #3 done"); return 80; })
    };
   auto conc = prot.on(0);
   conc.schedule();
   std::print("result: {}", conc.result());
}
