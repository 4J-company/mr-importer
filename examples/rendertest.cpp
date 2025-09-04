#include <filesystem>

#include <mr-manager/manager.hpp>

#include "render_polyscope.hpp"

#include "mr-importer/importer.hpp"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: mr-importer-mesh-example <filename>");
    exit(47);
  }

  auto handle = mr::Manager<mr::Model>::get().create("id", std::filesystem::path(argv[1]));
  render(handle->meshes);
}
