#include "pipe.hpp"

mr::Executor::Executor() noexcept {
  for (int i = 0; i < threadcount; i++) {
    threads.emplace_back(
      [this](const auto &token) {
        while (not token.stop_requested()) {
          group.execute_next_contract();
        }
      });
  }
}
