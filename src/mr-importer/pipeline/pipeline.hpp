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

  // NOTE: ImportPipeline should only be used by 1 thread by design
  template <typename T>
  struct Pipeline {
  public:
    using Step = bcpp::work_contract;

    std::atomic_flag asset_ready;
    std::function<void(void)> on_finish;
    mutable std::vector<Step> steps;
    mutable std::unique_ptr<T> object;

    Pipeline() noexcept = default;
    ~Pipeline() noexcept = default;

    Pipeline(std::function<void(void)> callable) {
      on_finish = std::move(callable);
    }

    Pipeline(T initial, std::function<void(void)> callable) {
      object = std::make_unique<T>(std::move(initial));
      on_finish = std::move(callable);
    }

    Pipeline(const Pipeline &) = delete;
    Pipeline& operator=(const Pipeline &) = delete;

    Pipeline & add_step(std::function<void(T*)> f) {
      auto step = Executor::get().group.create_contract(
        [this, f, index=steps.size()] {
          f(object.get());
          if (index + 1 < steps.size()) {
            steps[index + 1].schedule();
          }
          else {
            on_finish();
            asset_ready.test_and_set();
            asset_ready.notify_one();
          }
        }
      );
      steps.emplace_back(std::move(step));
      return *this;
    }

    const Pipeline & execute() const noexcept {
      schedule();
      wait();
      return *this;
    }

    const Pipeline & schedule() const noexcept {
      assert(steps.size() != 0);
      steps.front().schedule();
      return *this;
    }

    const Pipeline & wait() const noexcept {
      asset_ready.wait(false);
      return *this;
    }

    T&& asset() {
      assert(object);
      return std::move(*object.get());
    }

    const std::atomic_flag & ready() const noexcept {
      return asset_ready;
    }
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
    std::vector<Contract> contracts;

    Pipe() = default;

    Pipe(std::unique_ptr<VariantT> &&initial, std::vector<Contract> &&cs)
      : object(std::move(initial))
      , contracts(std::move(cs))
    {}

    void schedule() {
      // contracts.front().schedule();
      for (auto &c : contracts) {
        c.schedule();
        std::this_thread::sleep_for(1s);
      }
    }

    ResultT result() {
      return std::visit(
        [](auto &&obj) -> ResultT {
          if constexpr (std::is_same_v<decltype(obj), ResultT>) {
            return std::move(obj);
          } else {
            std::unreachable();
            return ResultT{};
          }
        },
        *object.get()
      );
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

      PipeT on(InitialInputT &&initial) {
        std::vector<Contract> contracts;
        std::unique_ptr<VariantT> object = std::make_unique<VariantT>(initial);

        auto append_contract = [&, obj_ptr = object.get()]<typename O, typename I>(std::function<O(I)> f) {
          auto func =
            [&]() {
            *obj_ptr = std::visit(
              Overloads{
                // NOTE: returning std::optional might be overhead
                //       since std::nullopt should never be returned
                //       due to static_assert but it seems reasonable
                //       and might protect against non-default-constructible types
                [ ](auto &&) -> std::optional<O> { std::unreachable(); return {}; },
                [f](I&& arg) -> std::optional<O> { return f(arg); }
              }, *obj_ptr
            ).value();
          };
          Contract contract = Executor::get().group.create_contract(std::move(func));
          contracts.emplace_back(std::move(contract));
        };

        std::apply(
          [&]<typename ...Fs>(Fs ... funcs) {
            (append_contract(funcs), ...);
          }, callables
        );

        return {std::move(object), std::move(contracts)};
      }
    };

    template <typename ...Is, typename ...Os>
      PipePrototype(std::function<Os(Is)> ...) -> PipePrototype<vtll::type_list<Is...>, vtll::type_list<Os...>>;
}
