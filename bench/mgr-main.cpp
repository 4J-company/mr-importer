#include <random>
#include <atomic>
#include <ranges>
#include <execution>

#include "log.hpp"
#include "manager/manager.hpp"

using namespace std::literals;

#define POLICY std::execution::par

struct Model : mr::ResourceBase<Model> {
  int a, b;

  Model() = delete;
  Model(Model &&) = default;
  Model& operator=(Model &&) = default;
  Model(const Model &) = default;
  Model& operator=(const Model &) = default;

  Model(int x, int y) : a(x), b(y) {
    volatile double res = 1.0;
    for (int i = 0; i < 1'000; i++) {
      res += std::sin(i) * std::sin(i) - std::tan((std::tgamma(i) + 1) * std::cos(i));
    }
  }
};

namespace mr {
  MR_DECLARE_HANDLE(Model)
};

template <std::mt19937::result_type min, std::mt19937::result_type max>
double randint()
{
  static std::random_device dev;
  static std::mt19937 rng(dev());
  static std::uniform_int_distribution<std::mt19937::result_type> dist(
    min, max); // distribution in range [1, 6]
  return dist(rng);
}

void manager_fuzz() {
  constexpr unsigned long long modelCount = 100'000;
  constexpr auto io = std::ranges::iota_view(0ull, modelCount);

  std::vector<mr::ModelHandle> models;
  models.resize(modelCount);

  std::atomic<int> cnt = 0;

  std::for_each(
      POLICY,
      io.begin(),
      io.end() - modelCount / 2,
      [&](auto i) {
        models[i] = mr::ResourceManager<Model>::get().create(mr::unnamed, i + 1, i + 2);
      }
    );

  std::for_each(
      POLICY,
      io.begin(),
      io.end(),
      [&](auto) {
        int t = randint<0, modelCount - 1>();
        auto res = mr::ResourceManager<Model>::get().find(std::to_string(t));
        if (res) {
          MR_INFO("Found: {}", t);
          cnt++;
        }
      }
      );

  std::for_each(
      POLICY,
      io.begin(),
      io.end(),
      [&](auto i) {
          int t1 = randint<0, modelCount - 1>();
          auto res1 = mr::ResourceManager<Model>::get().create(std::to_string(t1), t1 + 1, t1 + 2);

          int t2 = randint<0, modelCount - 1>();
          auto res2 = mr::ResourceManager<Model>::get().find(std::to_string(t2));
          if (res2) {
            cnt++;
          }
        }
      );

  MR_INFO("Found: {}", cnt / (double)io.size());
}

int main() {
  manager_fuzz();
}
