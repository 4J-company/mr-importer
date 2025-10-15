#include <filesystem>

#include "mr-importer/importer.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: mr-importer-shader-example <filename>");
    exit(47);
  }

  mr::Shader shader {std::filesystem::path(argv[1])};
  printf("Pointer: %p\n", shader.spirv.get());
  printf("Size: %zu\n", shader.spirv.size());
}
