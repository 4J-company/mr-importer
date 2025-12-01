#include <filesystem>

#include "render_polyscope.hpp"

#include "mr-importer/importer.hpp"

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: mr-importer-mesh-example <filename> <lodnumber> <enable-meshlets>");
    exit(47);
  }

  const char * filepath = argv[1];
  int lodnumber = argc < 3 ? 0 : std::atoi(argv[2]);
  bool generate_and_render_meshlets = argc < 4 ? true : std::atoi(argv[3]);

  mr::Options options = mr::Options(mr::Options::All & (generate_and_render_meshlets ? mr::Options::All : ~mr::Options::GenerateMeshlets));

  std::optional<mr::Model> model = mr::import(filepath, options);

  // make sure texture are readable
  for (const auto& mtl : model->materials) {
    for (const auto& tex : mtl.textures) {
      for (int i = 0; i < tex.image.pixels.size(); i++) {
        volatile auto tmp = tex.image.pixels[i];
      }
    }
  }

  if (generate_and_render_meshlets) {
    render_meshlets(model->meshes, lodnumber);
  }
  else {
    render(model->meshes, lodnumber);
  }
}
