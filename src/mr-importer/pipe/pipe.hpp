#pragma once

#include <atomic>
#include <cassert>
#include <functional>
#include <thread>
#include <vector>

#include <library/work_contract/work_contract.h>

#include "def.hpp"

namespace mr {
  // function to specialize
  template <typename ResultT, typename ...Args> auto make_pipe_prototype();

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

  template <typename ResultT>
  struct PipeBase {
    std::vector<Contract> contracts {};
    std::atomic_flag completion_flag = false;

    PipeBase() noexcept = default;

    PipeBase(const PipeBase&) = delete;
    PipeBase& operator=(const PipeBase&) = delete;

    PipeBase(PipeBase&& other) noexcept {
      contracts = std::move(other.contracts);
      if (other.completion_flag.test()) {
        completion_flag.test_and_set();
      } else {
        completion_flag.clear();
      }
    }
    PipeBase& operator=(PipeBase&& other) noexcept {
      contracts = std::move(other.contracts);
      if (other.completion_flag.test()) {
        completion_flag.test_and_set();
      } else {
        completion_flag.clear();
      }
      return *this;
    }

    virtual void update_object() noexcept = 0;

    PipeBase & schedule() noexcept {
      update_object();
      contracts.front().schedule();
      return *this;
    }

    PipeBase & wait() noexcept {
      completion_flag.wait(false);
      return *this;
    }

    PipeBase & execute() noexcept {
      return schedule().wait();
    }

    [[nodiscard]] virtual ResultT result() noexcept = 0;
  };

  template <typename ResultT> using PipeHandle = std::unique_ptr<PipeBase<ResultT>>;

  template <typename VariantT, typename ResultT>
  struct Pipe : PipeBase<ResultT> {
    VariantT _initial;
    std::unique_ptr<VariantT> object;

    Pipe() = default;

    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;

    Pipe(Pipe&&) = default;
    Pipe& operator=(Pipe&&) = default;

    Pipe(VariantT &&initial)
      : _initial(initial)
      , object(std::make_unique<VariantT>(_initial))
    {}

    virtual void update_object() noexcept override final {
      if (!object) {
        object = std::make_unique<VariantT>(_initial);
      }
    }

    [[nodiscard]] ResultT result() noexcept override final {
      return std::move(std::get<ResultT>(*object.get()));
    }

    template<size_t StageIdx, size_t EndIdx, typename InputT, typename OutputT>
      void create_contract(const std::function<OutputT(InputT)> &stage) noexcept {
        auto contract = Executor::get().group.create_contract(
          [this, stage]() {
            auto result = stage(std::move(std::get<InputT>(*object.get())));
            object->template emplace<OutputT>(std::move(result));

            if constexpr (StageIdx + 1 < EndIdx) {
              PipeBase<ResultT>::contracts[StageIdx + 1].schedule();
            } else {
              PipeBase<ResultT>::completion_flag.test_and_set(std::memory_order_release);
              PipeBase<ResultT>::completion_flag.notify_one();
            }
          }
        );
        PipeBase<ResultT>::contracts.emplace_back(std::move(contract));
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
        >::value, "Invalid transform chain (type mismatch)");

      using InitialInputT = vtll::front<InputsListT>;
      using FinalOutputT = vtll::back<OutputsListT>;

      using UniqueInputOutputListT = vtll::remove_duplicates<InputOutputListT>;
      using VariantT = vtll::to_variant<UniqueInputOutputListT>;
      using PipeT = Pipe<VariantT, FinalOutputT>;
      using PipeHandleT = PipeHandle<FinalOutputT>;

      std::tuple<std::function<Os(Is)>...> callables;

      PipePrototype() = default;
      PipePrototype(std::function<Os(Is)> ...cs) : callables(cs...) {}

      constexpr PipeHandleT on(InitialInputT &&initial) {
        constexpr size_t EndIdx = sizeof...(Os);

        std::unique_ptr<PipeT> pipe = std::make_unique<PipeT>(std::move(initial));
        [this, &pipe]<size_t... Indices>(std::index_sequence<Indices...>) {
          (pipe->template create_contract<Indices, EndIdx>(std::get<Indices>(callables)), ...);
        }(std::make_index_sequence<EndIdx>{});

        return pipe;
      }

      // appends `on_finish` function as the last contract
      PipeHandleT on(InitialInputT &&initial, std::function<void(void)> on_finish) {
        constexpr size_t EndIdx = sizeof...(Os) + 1;

        std::unique_ptr<PipeT> pipe = std::make_unique<PipeT>(std::move(initial));
        [this, &pipe]<size_t... Indices>(std::index_sequence<Indices...>) {
          (pipe->template create_contract<Indices, EndIdx>(std::get<Indices>(callables)), ...);
        }(std::make_index_sequence<EndIdx>{});

        pipe->template create_contract<EndIdx - 1, EndIdx>(on_finish);

        return pipe;
      }

    };

    template <typename ...Is, typename ...Os>
      PipePrototype(std::function<Os(Is)> ...) -> PipePrototype<vtll::type_list<Is...>, vtll::type_list<Os...>>;
}
