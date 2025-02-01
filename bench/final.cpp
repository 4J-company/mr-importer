#include "mr-importer/def.hpp"
#include "mr-importer/manager/manager.hpp"

#include "assets.hpp"

int main() {
  auto mgr = mr::ResourceManager<mr::Image>::get();
  auto image = mgr.create(std::fs::current_path());

  std::print("{}", image->value);
}
