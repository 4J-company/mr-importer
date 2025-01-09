#pragma once

#include <cassert>
#include <thread>
#include <vector>

#include <library/work_contract/work_contract.h>

#include "log.hpp"

namespace mr {
  // NOTE: ImportPipeline should only be used by 1 thread by design
  template <typename T>

  struct ImportPipeline {
    std::vector<std::jthread> threads;

    bcpp::work_contract_group group;
    std::atomic_bool asset_ready;
    mutable std::vector<bcpp::work_contract> contracts;
    mutable std::unique_ptr<T> object;

    ImportPipeline() {
      for (int i = 0; i < 8; i++) {
        threads.emplace_back(
          [this](const auto &token) {
            while (not token.stop_requested()) {
              group.execute_next_contract();
            }
          });
      }
    }
    ImportPipeline(const T &initial) : ImportPipeline() {
      object = std::make_unique<T>(initial);
    }
    ImportPipeline(const ImportPipeline &) = delete;
    ImportPipeline& operator=(const ImportPipeline &) = delete;

    ~ImportPipeline() noexcept = default;

    ImportPipeline & add_node(std::function<void(T*)> f) {
      contracts.emplace_back(group.create_contract(
        [this, f, index=contracts.size()] {
          f(object.get());
          if (index + 1 < contracts.size()) {
            MR_INFO("current WC index: {}", index);
            contracts[index + 1].schedule();
          }
          else {
            asset_ready = true;
            asset_ready.notify_one();
          }
        }));
      return *this;
    }

    void execute() const noexcept {
      schedule();
      asset_ready.wait(false);
    }

    void schedule() const noexcept {
      assert(contracts.size() != 0);
      contracts.front().schedule();
    }

    T asset() {
      assert(object);
      return std::move(*object.get());
    }

    const std::atomic_bool & ready() const noexcept {
      return asset_ready;
    }
  };
}
