#include <filesystem>

#include "mr-importer/importer.hpp"

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: mr-importer-shader-example <filename0> <filename1> ...");
    exit(47);
  }

  for (int i = 1; i < argc; i++) {
    auto shaders = mr::compile(argv[i])
      .value_or(std::vector<mr::Shader>());

    printf("Number of shaders for file %s: %zu\n", argv[i], shaders.size());
    for (const auto &shader : shaders) {
      printf("\tPointer: %p\n", shader.spirv.get());
      printf("\tSize: %zu\n", shader.spirv.size());
    }
  }
}
