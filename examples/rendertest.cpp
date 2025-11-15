#include <filesystem>

#include "render_polyscope.hpp"

#include "mr-importer/importer.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: mr-importer-mesh-example <filename>");
    exit(47);
  }

  mr::Model model {std::filesystem::path(argv[1])};

  // make sure texture are readable
  for (const auto& mtl : model.materials) {
    for (const auto& tex : mtl.textures) {
      for (int i = 0; i < tex.image.pixels.size(); i++) {
        volatile auto tmp = tex.image.pixels[i];
      }
    }
  }

  render(model.meshes);
}
