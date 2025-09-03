#include <filesystem>

#include <mr-manager/manager.hpp>

#include "mr-importer/importer.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: mr-importer-shader-example <filename>");
    exit(47);
  }

  auto handle = mr::Manager<mr::Shader>::get().create("id", std::filesystem::path(argv[1]));
}
