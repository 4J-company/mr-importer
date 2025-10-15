#include <filesystem>

#include "render_polyscope.hpp"

#include "mr-importer/importer.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: mr-importer-mesh-example <filename>");
    exit(47);
  }

  mr::Model model {std::filesystem::path(argv[1])};
  render(model.meshes);
}
