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

  mr::Options options = mr::Options::All;
  if (!generate_and_render_meshlets) {
    mr::disable(options, mr::Options::GenerateMeshlets);
  }

  std::optional<mr::Model> model = mr::import(filepath, options);

  int triangle_count[8] = {};
  for (const auto& mesh : model->meshes) {
    for (int i = 0; i < mesh.lods.size(); i++) {
      triangle_count[i] += mesh.lods[i].indices.size() / 3;
    }
  }
  int i = 0;
  while (triangle_count[i] != 0) {
    printf("LOD[%d] triangle count: %d\n", i, triangle_count[i]);
    i++;
  }

  if (generate_and_render_meshlets) {
    render_meshlets(model.value(), lodnumber);
  }
  else {
    render(model.value(), lodnumber);
  }
}
