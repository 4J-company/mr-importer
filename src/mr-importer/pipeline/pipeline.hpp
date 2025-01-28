#pragma once

#include <atomic>
#include <cassert>
#include <functional>
#include <thread>
#include <vector>

#include <library/work_contract/work_contract.h>

#include "def.hpp"

namespace mr {
  struct Executor {
  public:
    inline static constexpr int threadcount = 8;

    bcpp::work_contract_group group;
    std::vector<std::jthread> threads;

    static Executor & get() noexcept {
      static Executor executor {};
      return executor;
    }

  private:
    Executor() noexcept;
  };

  template <typename ...> struct PipePrototype;

  using Contract = bcpp::work_contract;

  /* NOTE:
   * Investigate why this doesn't work
   *   struct Contract : bcpp::work_contract {
   *     using bcpp::work_contract::work_contract;
   *     using bcpp::work_contract::operator=;
   *   };
  */

  template <typename VariantT, typename ResultT>
  struct Pipe {
    std::unique_ptr<VariantT> object;
    std::vector<Contract> contracts {};
    std::atomic_flag completion_flag = false;

    Pipe() = default;

    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;

    Pipe(Pipe&& other) noexcept {
      object = std::move(other.object);
      contracts = std::move(other.contracts);
      if (other.completion_flag.test()) {
        completion_flag.test_and_set();
      } else {
        completion_flag.clear();
      }
    }
    Pipe& operator=(Pipe&& other) noexcept {
      object = std::move(other.object);
      contracts = std::move(other.contracts);
      if (other.completion_flag.test()) {
        completion_flag.test_and_set();
      } else {
        completion_flag.clear();
      }
      return *this;
    }

    Pipe(VariantT &&initial)
      : object(std::make_unique<VariantT>(std::move(initial)))
    {}

    void schedule() {
      contracts.front().schedule();
    }

    ResultT result() {
      completion_flag.wait(false);
      return std::get<ResultT>(*object.get());
    }

    template<size_t StageIdx, size_t EndIdx, typename InputT, typename OutputT>
      void create_contract(const std::function<OutputT(InputT)> &stage) {
        auto contract = Executor::get().group.create_contract(
          [this, stage]() {
            auto result = stage(std::move(std::get<InputT>(*object.get())));
            object->template emplace<OutputT>(std::move(result));

            if constexpr (StageIdx + 1 < EndIdx) {
              contracts[StageIdx + 1].schedule();
            } else {
              completion_flag.test_and_set(std::memory_order_release);
              completion_flag.notify_one();
            }
          }
        );
        contracts.emplace_back(std::move(contract));
      }
  };

  template <typename ...Is, typename ...Os>
    struct PipePrototype<vtll::type_list<Is...>, vtll::type_list<Os...>> {

      using InputsListT = vtll::type_list<Is...>;
      using OutputsListT = vtll::type_list<Os...>;
      using InputOutputListT = vtll::type_list<Is..., Os...>;

      static_assert(vtll::is_same_list<
          vtll::erase_Nth<InputsListT, 0>,
          vtll::erase_Nth<OutputsListT, vtll::size<OutputsListT>::value - 1>
        >::value, "Invalid transform chain");

      using InitialInputT = vtll::front<InputsListT>;
      using FinalOutputT = vtll::back<OutputsListT>;

      using UniqueInputOutputListT = vtll::remove_duplicates<InputOutputListT>;
      using VariantT = vtll::to_variant<UniqueInputOutputListT>;
      using PipeT = Pipe<VariantT, FinalOutputT>;

      std::tuple<std::function<Os(Is)>...> callables;

      PipePrototype() = default;
      PipePrototype(std::function<Os(Is)> ...cs) : callables(cs...) {}

      constexpr PipeT on(InitialInputT &&initial) {
        PipeT pipe{std::move(initial)};
        constexpr size_t EndIdx = sizeof...(Os);
        [this, &pipe]<size_t... Indices>(std::index_sequence<Indices...>) {
          (pipe.template create_contract<Indices, EndIdx>(std::get<Indices>(callables)), ...);
        }(std::make_index_sequence<EndIdx>{});
        return pipe;
      }
    };

    template <typename ...Is, typename ...Os>
      PipePrototype(std::function<Os(Is)> ...) -> PipePrototype<vtll::type_list<Is...>, vtll::type_list<Os...>>;
}
